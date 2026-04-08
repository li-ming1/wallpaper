#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace wallpaper {

class PresentSampleWindow final {
 public:
  static constexpr std::size_t kCapacity = 64;

  void PushSample(const double value) noexcept {
    const std::size_t tail = (head_ + size_) % samples_.size();
    const std::uint16_t bucket = QuantizeSampleBucket(value);
    if (size_ == samples_.size()) {
      const std::uint16_t oldBucket = sampleBuckets_[head_];
      if (histogram_[oldBucket] > 0) {
        --histogram_[oldBucket];
      }
      samples_[head_] = value;
      sampleBuckets_[head_] = bucket;
      ++histogram_[bucket];
      head_ = (head_ + 1U) % samples_.size();
      return;
    }
    samples_[tail] = value;
    sampleBuckets_[tail] = bucket;
    ++histogram_[bucket];
    ++size_;
  }

  void Clear() noexcept {
    head_ = 0;
    size_ = 0;
    histogram_.fill(0);
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

    const std::size_t rank = ((size_ - 1U) * 95U) / 100U + 1U;
    std::size_t seen = 0;
    std::size_t bucket = 0;
    for (; bucket < histogram_.size(); ++bucket) {
      seen += histogram_[bucket];
      if (seen >= rank) {
        break;
      }
    }
    const double p95 = DequantizeSampleBucket(bucket);
    Clear();
    return p95;
  }

 private:
  static constexpr std::size_t kHistogramScale = 8;  // 0.125ms bins.
  static constexpr std::size_t kHistogramMaxMs = 512;
  static constexpr std::size_t kHistogramBinCount =
      kHistogramMaxMs * kHistogramScale + 2U;  // +overflow bucket.

  [[nodiscard]] static std::uint16_t QuantizeSampleBucket(const double value) noexcept {
    if (!(value >= 0.0)) {
      return 0;
    }
    const double scaled = value * static_cast<double>(kHistogramScale);
    std::size_t bucket = scaled > 0.0 ? static_cast<std::size_t>(scaled) : 0U;
    const std::size_t maxNormalBucket = kHistogramBinCount - 2U;
    if (bucket > maxNormalBucket) {
      bucket = kHistogramBinCount - 1U;
    }
    return static_cast<std::uint16_t>(bucket);
  }

  [[nodiscard]] static double DequantizeSampleBucket(const std::size_t bucket) noexcept {
    if (bucket >= kHistogramBinCount - 1U) {
      return static_cast<double>(kHistogramMaxMs);
    }
    return static_cast<double>(bucket) / static_cast<double>(kHistogramScale);
  }

  std::array<double, kCapacity> samples_{};
  std::array<std::uint16_t, kCapacity> sampleBuckets_{};
  std::array<std::uint16_t, kHistogramBinCount> histogram_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace wallpaper
