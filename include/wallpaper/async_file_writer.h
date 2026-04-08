#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "wallpaper/fixed_task_queue.h"

namespace wallpaper {

class AsyncFileWriter final {
 public:
  struct Task final {
    std::filesystem::path path;
    bool append = true;
    std::string data;
  };

  explicit AsyncFileWriter(std::size_t capacity = 256, bool startWorker = true);
  ~AsyncFileWriter();

  AsyncFileWriter(const AsyncFileWriter&) = delete;
  AsyncFileWriter& operator=(const AsyncFileWriter&) = delete;

  [[nodiscard]] bool Enqueue(Task task);
  void FlushAndStop();

  [[nodiscard]] std::size_t dropped_count() const noexcept { return droppedCount_.load(); }
  [[nodiscard]] std::size_t failure_count() const noexcept { return failureCount_.load(); }

 private:
  void Run();
  void DrainQueue();
  bool WriteTask(const Task& task);
  std::ofstream* AcquireAppendStream(const std::filesystem::path& path);
  [[nodiscard]] std::size_t AcquireStreamCacheSlot() noexcept;
  void InitializeStreamCacheSlots() noexcept;
  void AttachStreamCacheToLruTail(std::size_t index) noexcept;
  void DetachStreamCacheFromLru(std::size_t index) noexcept;
  void TouchStreamCacheEntry(std::size_t index) noexcept;
  void InvalidateCachedStream(const std::filesystem::path& path);
  void ClearStreamCache();
  void ResetStreamCacheEntry(std::size_t index);

  struct StreamCacheEntry final {
    std::filesystem::path path;
    std::ofstream stream;
    std::size_t prevIndex = 0;
    std::size_t nextIndex = 0;
    bool occupied = false;
  };

  static constexpr std::size_t kMaxCachedStreams = 4;
  static constexpr std::size_t kInvalidCacheIndex = static_cast<std::size_t>(-1);

  const std::size_t capacity_;
  const bool workerStarted_;
  FixedTaskQueue<Task> queue_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  bool stopping_ = false;
  std::thread worker_;
  std::atomic<std::size_t> droppedCount_{0};
  std::atomic<std::size_t> failureCount_{0};
  std::array<StreamCacheEntry, kMaxCachedStreams> streamCache_{};
  std::unordered_map<std::filesystem::path, std::size_t> streamCacheIndex_;
  std::size_t streamCacheSize_ = 0;
  // freeCacheHead_ 管理空闲槽单链表；lruHead_/lruTail_ 管理已占用槽双向 LRU 链。
  std::size_t freeCacheHead_ = kInvalidCacheIndex;
  std::size_t lruHead_ = kInvalidCacheIndex;
  std::size_t lruTail_ = kInvalidCacheIndex;
};

}  // namespace wallpaper
