#pragma once

#include <cstddef>

#include "wallpaper/fixed_task_queue.h"
#include "wallpaper/interfaces.h"

namespace wallpaper {

class TrayActionQueue final {
 public:
  explicit TrayActionQueue(const std::size_t capacity) : queue_(capacity) {}

  [[nodiscard]] bool Enqueue(TrayAction action) {
    queue_.PushBackOverwrite(std::move(action));
    return true;
  }

  [[nodiscard]] bool Dequeue(TrayAction* const action) { return queue_.PopFront(action); }

  [[nodiscard]] bool Empty() const noexcept { return queue_.Empty(); }

 private:
  FixedTaskQueue<TrayAction> queue_;
};

}  // namespace wallpaper
