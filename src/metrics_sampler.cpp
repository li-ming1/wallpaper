#include "wallpaper/metrics_sampler.h"

namespace wallpaper {

MetricsSampler::MetricsSampler(const std::size_t maxSamples)
    : maxSamples_(maxSamples == 0 ? 1 : maxSamples) {
  samples_.resize(maxSamples_);
}

void MetricsSampler::PushSample(const RuntimeMetrics sample) {
  samples_[head_] = sample;
  head_ = (head_ + 1U) % maxSamples_;
  if (size_ < maxSamples_) {
    ++size_;
  }
}

RuntimeMetrics MetricsSampler::Latest() const {
  if (size_ == 0) {
    return {};
  }
  const std::size_t lastIndex = (head_ + maxSamples_ - 1U) % maxSamples_;
  return samples_[lastIndex];
}

std::vector<RuntimeMetrics> MetricsSampler::Snapshot() const {
  std::vector<RuntimeMetrics> snapshot;
  snapshot.reserve(size_);
  if (size_ == 0) {
    return snapshot;
  }

  const std::size_t oldestIndex = (head_ + maxSamples_ - size_) % maxSamples_;
  for (std::size_t i = 0; i < size_; ++i) {
    const std::size_t index = (oldestIndex + i) % maxSamples_;
    snapshot.push_back(samples_[index]);
  }
  return snapshot;
}

}  // namespace wallpaper
