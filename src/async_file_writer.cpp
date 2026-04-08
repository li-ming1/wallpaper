#include "wallpaper/async_file_writer.h"
#include "wallpaper/fixed_task_queue.h"

#include <algorithm>

namespace wallpaper {
namespace {

void EnsureParentDirectory(const std::filesystem::path& path) {
  const auto parent = path.parent_path();
  if (parent.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
}

}  // namespace

AsyncFileWriter::AsyncFileWriter(const std::size_t capacity, const bool startWorker)
    : capacity_(std::max<std::size_t>(1, capacity)),
      workerStarted_(startWorker),
      queue_(capacity_) {
  InitializeStreamCacheSlots();
  if (workerStarted_) {
    worker_ = std::thread(&AsyncFileWriter::Run, this);
  }
}

AsyncFileWriter::~AsyncFileWriter() { FlushAndStop(); }

bool AsyncFileWriter::Enqueue(Task task) {
  std::lock_guard<std::mutex> lock(mu_);
  if (stopping_) {
    return false;
  }
  if (queue_.Full()) {
    const Task* const oldest = queue_.Front();
    const bool dropIncoming = oldest != nullptr && task.append && !oldest->append;
    droppedCount_.fetch_add(1, std::memory_order_relaxed);
    if (dropIncoming) {
      // 满队列时优先保留 truncate（append=false）任务，避免关键配置写被日志流量挤掉。
      return true;
    }
  }
  queue_.PushBackOverwrite(std::move(task));
  cv_.notify_one();
  return true;
}

void AsyncFileWriter::FlushAndStop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  } else {
    DrainQueue();
  }
  ClearStreamCache();
}

void AsyncFileWriter::Run() {
  for (;;) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this] { return stopping_ || !queue_.Empty(); });
      if (queue_.Empty()) {
        if (stopping_) {
          break;
        }
        continue;
      }
      (void)queue_.PopFront(&task);
    }
    if (!WriteTask(task)) {
      failureCount_.fetch_add(1, std::memory_order_relaxed);
    }
  }
  DrainQueue();
}

void AsyncFileWriter::DrainQueue() {
  for (;;) {
    Task task;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!queue_.PopFront(&task)) {
        break;
      }
    }
    if (!WriteTask(task)) {
      failureCount_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

bool AsyncFileWriter::WriteTask(const Task& task) {
  EnsureParentDirectory(task.path);
  if (!task.append) {
    InvalidateCachedStream(task.path);
    std::ofstream out(task.path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      return false;
    }
    out.write(task.data.data(), static_cast<std::streamsize>(task.data.size()));
    return static_cast<bool>(out);
  }

  std::ofstream* const stream = AcquireAppendStream(task.path);
  if (stream == nullptr) {
    return false;
  }
  stream->write(task.data.data(), static_cast<std::streamsize>(task.data.size()));
  if (!static_cast<bool>(*stream)) {
    InvalidateCachedStream(task.path);
    return false;
  }
  return true;
}

void AsyncFileWriter::InitializeStreamCacheSlots() noexcept {
  for (std::size_t index = 0; index < streamCache_.size(); ++index) {
    StreamCacheEntry& entry = streamCache_[index];
    entry.path.clear();
    entry.occupied = false;
    entry.prevIndex = kInvalidCacheIndex;
    entry.nextIndex = index + 1U < streamCache_.size() ? index + 1U : kInvalidCacheIndex;
  }
  streamCacheIndex_.clear();
  streamCacheSize_ = 0;
  freeCacheHead_ = streamCache_.empty() ? kInvalidCacheIndex : 0U;
  lruHead_ = kInvalidCacheIndex;
  lruTail_ = kInvalidCacheIndex;
}

void AsyncFileWriter::AttachStreamCacheToLruTail(const std::size_t index) noexcept {
  if (index >= streamCache_.size()) {
    return;
  }
  StreamCacheEntry& entry = streamCache_[index];
  entry.prevIndex = lruTail_;
  entry.nextIndex = kInvalidCacheIndex;
  if (lruTail_ != kInvalidCacheIndex) {
    streamCache_[lruTail_].nextIndex = index;
  } else {
    lruHead_ = index;
  }
  lruTail_ = index;
}

void AsyncFileWriter::DetachStreamCacheFromLru(const std::size_t index) noexcept {
  if (index >= streamCache_.size()) {
    return;
  }
  StreamCacheEntry& entry = streamCache_[index];
  const std::size_t prev = entry.prevIndex;
  const std::size_t next = entry.nextIndex;

  if (prev != kInvalidCacheIndex) {
    streamCache_[prev].nextIndex = next;
  } else if (lruHead_ == index) {
    lruHead_ = next;
  }

  if (next != kInvalidCacheIndex) {
    streamCache_[next].prevIndex = prev;
  } else if (lruTail_ == index) {
    lruTail_ = prev;
  }

  entry.prevIndex = kInvalidCacheIndex;
  entry.nextIndex = kInvalidCacheIndex;
}

void AsyncFileWriter::TouchStreamCacheEntry(const std::size_t index) noexcept {
  if (index >= streamCache_.size() || lruTail_ == index) {
    return;
  }
  DetachStreamCacheFromLru(index);
  AttachStreamCacheToLruTail(index);
}

std::size_t AsyncFileWriter::AcquireStreamCacheSlot() noexcept {
  // 先走空闲链；无空闲时直接淘汰 LRU 头结点，保持分配/淘汰 O(1)。
  if (freeCacheHead_ == kInvalidCacheIndex) {
    if (lruHead_ == kInvalidCacheIndex) {
      return kInvalidCacheIndex;
    }
    ResetStreamCacheEntry(lruHead_);
  }
  if (freeCacheHead_ == kInvalidCacheIndex) {
    return kInvalidCacheIndex;
  }

  const std::size_t index = freeCacheHead_;
  StreamCacheEntry& entry = streamCache_[index];
  freeCacheHead_ = entry.nextIndex;
  entry.prevIndex = kInvalidCacheIndex;
  entry.nextIndex = kInvalidCacheIndex;
  return index;
}

void AsyncFileWriter::ResetStreamCacheEntry(const std::size_t index) {
  if (index >= streamCache_.size()) {
    return;
  }
  StreamCacheEntry& entry = streamCache_[index];
  if (!entry.occupied) {
    return;
  }
  streamCacheIndex_.erase(entry.path);
  DetachStreamCacheFromLru(index);
  if (entry.stream.is_open()) {
    entry.stream.close();
  }
  entry.stream.clear();
  entry.path.clear();
  entry.occupied = false;
  entry.nextIndex = freeCacheHead_;
  entry.prevIndex = kInvalidCacheIndex;
  freeCacheHead_ = index;
  if (streamCacheSize_ > 0) {
    --streamCacheSize_;
  }
}

std::ofstream* AsyncFileWriter::AcquireAppendStream(const std::filesystem::path& path) {
  const auto cacheIt = streamCacheIndex_.find(path);
  if (cacheIt != streamCacheIndex_.end()) {
    const std::size_t cacheIndex = cacheIt->second;
    if (cacheIndex < streamCache_.size()) {
      StreamCacheEntry& cached = streamCache_[cacheIndex];
      if (cached.occupied && cached.path == path && cached.stream.is_open()) {
        TouchStreamCacheEntry(cacheIndex);
        return &cached.stream;
      }
      ResetStreamCacheEntry(cacheIndex);
    } else {
      streamCacheIndex_.erase(cacheIt);
    }
  }

  const std::size_t insertIndex = AcquireStreamCacheSlot();
  if (insertIndex == kInvalidCacheIndex) {
    return nullptr;
  }

  StreamCacheEntry& entry = streamCache_[insertIndex];
  entry.path = path;
  entry.stream.open(entry.path, std::ios::binary | std::ios::app);
  if (!entry.stream.is_open()) {
    entry.stream.clear();
    entry.path.clear();
    entry.occupied = false;
    entry.nextIndex = freeCacheHead_;
    entry.prevIndex = kInvalidCacheIndex;
    freeCacheHead_ = insertIndex;
    return nullptr;
  }
  entry.occupied = true;
  AttachStreamCacheToLruTail(insertIndex);
  streamCacheIndex_[entry.path] = insertIndex;
  ++streamCacheSize_;
  return &entry.stream;
}

void AsyncFileWriter::InvalidateCachedStream(const std::filesystem::path& path) {
  const auto cacheIt = streamCacheIndex_.find(path);
  if (cacheIt == streamCacheIndex_.end()) {
    return;
  }
  ResetStreamCacheEntry(cacheIt->second);
}

void AsyncFileWriter::ClearStreamCache() {
  for (std::size_t index = 0; index < streamCache_.size(); ++index) {
    StreamCacheEntry& entry = streamCache_[index];
    if (entry.stream.is_open()) {
      entry.stream.close();
    }
    entry.stream.clear();
  }
  InitializeStreamCacheSlots();
}

}  // namespace wallpaper
