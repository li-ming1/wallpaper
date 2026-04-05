#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

  const std::size_t capacity_;
  const bool workerStarted_;
  std::vector<Task> queue_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  bool stopping_ = false;
  std::thread worker_;
  std::atomic<std::size_t> droppedCount_{0};
  std::atomic<std::size_t> failureCount_{0};
};

}  // namespace wallpaper
