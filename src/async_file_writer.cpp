#include "wallpaper/async_file_writer.h"
#include "wallpaper/fixed_task_queue.h"

#include <algorithm>
#include <fstream>

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
  std::ios::openmode mode = std::ios::binary | (task.append ? std::ios::app : std::ios::trunc);
  std::ofstream out(task.path, mode);
  if (!out.is_open()) {
    return false;
  }
  out.write(task.data.data(), static_cast<std::streamsize>(task.data.size()));
  return static_cast<bool>(out);
}

}  // namespace wallpaper
