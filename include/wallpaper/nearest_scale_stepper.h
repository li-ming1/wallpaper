#pragma once

#include <cstdint>

namespace wallpaper {

class NearestScaleStepper final {
 public:
  constexpr NearestScaleStepper(const int srcExtent, const int dstExtent) noexcept
      : srcExtent_(srcExtent > 0 ? srcExtent : 0),
        dstExtent_(dstExtent > 0 ? dstExtent : 0),
        maxSourceIndex_(srcExtent_ > 0 ? srcExtent_ - 1 : 0) {}

  [[nodiscard]] constexpr int CurrentSourceIndex() const noexcept { return sourceIndex_; }

  constexpr void Advance() noexcept {
    if (srcExtent_ <= 0 || dstExtent_ <= 0) {
      return;
    }

    accumulator_ += static_cast<std::int64_t>(srcExtent_);
    sourceIndex_ += static_cast<int>(accumulator_ / static_cast<std::int64_t>(dstExtent_));
    accumulator_ %= static_cast<std::int64_t>(dstExtent_);
    if (sourceIndex_ > maxSourceIndex_) {
      sourceIndex_ = maxSourceIndex_;
    }
  }

 private:
  int srcExtent_ = 0;
  int dstExtent_ = 0;
  int maxSourceIndex_ = 0;
  int sourceIndex_ = 0;
  std::int64_t accumulator_ = 0;
};

}  // namespace wallpaper
