#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace wallpaper {

template <typename T>
class FixedTaskQueue final {
 public:
  explicit FixedTaskQueue(const std::size_t capacity)
      : capacity_(std::max<std::size_t>(1, capacity)), slots_(capacity_) {}

  [[nodiscard]] std::size_t Size() const noexcept { return size_; }
  [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }
  [[nodiscard]] bool Empty() const noexcept { return size_ == 0; }
  [[nodiscard]] bool Full() const noexcept { return size_ == capacity_; }

  [[nodiscard]] T* Front() noexcept {
    if (size_ == 0) {
      return nullptr;
    }
    return &slots_[head_];
  }

  [[nodiscard]] const T* Front() const noexcept {
    if (size_ == 0) {
      return nullptr;
    }
    return &slots_[head_];
  }

  [[nodiscard]] bool PushBack(T value) {
    PushBackOverwrite(std::move(value));
    return true;
  }

  void PushBackOverwrite(T value) {
    const std::size_t tail = (head_ + size_) % capacity_;
    if (size_ == capacity_) {
      slots_[head_] = std::move(value);
      head_ = (head_ + 1U) % capacity_;
      return;
    }
    slots_[tail] = std::move(value);
    ++size_;
  }

  bool PopFront(T* const out) {
    if (out == nullptr || size_ == 0) {
      return false;
    }
    *out = std::move(slots_[head_]);
    head_ = (head_ + 1U) % capacity_;
    --size_;
    return true;
  }

 private:
  std::size_t capacity_ = 1;
  std::vector<T> slots_;
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace wallpaper
