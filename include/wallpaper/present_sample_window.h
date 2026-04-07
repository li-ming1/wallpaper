#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace wallpaper {

class PresentSampleWindow final {
 public:
  static constexpr std::size_t kCapacity = 64;

  void PushSample(const double value) noexcept {
    const std::size_t tail = (head_ + size_) % samples_.size();
    if (size_ == samples_.size()) {
      samples_[head_] = value;
      head_ = (head_ + 1U) % samples_.size();
      return;
    }
    samples_[tail] = value;
    ++size_;
  }

  void Clear() noexcept {
    head_ = 0;
    size_ = 0;
  }

  [[nodiscard]] std::size_t Size() const noexcept { return size_; }

  [[nodiscard]] double OldestSample() const noexcept {
    return size_ == 0 ? 0.0 : samples_[head_];
  }

  [[nodiscard]] double NewestSample() const noexcept {
    return size_ == 0 ? 0.0 : samples_[(head_ + size_ - 1U) % samples_.size()];
  }

  [[nodiscard]] double TakeP95AndClear() noexcept {
    if (size_ == 0) {
      return 0.0;
    }

    std::array<double, kCapacity> ordered{};
    for (std::size_t index = 0; index < size_; ++index) {
      ordered[index] = samples_[(head_ + index) % samples_.size()];
    }

    const std::size_t p95Index = ((size_ - 1U) * 95U) / 100U;
    std::nth_element(ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(p95Index),
                     ordered.begin() + static_cast<std::ptrdiff_t>(size_));
    const double p95 = ordered[p95Index];
    Clear();
    return p95;
  }

 private:
  std::array<double, kCapacity> samples_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace wallpaper
