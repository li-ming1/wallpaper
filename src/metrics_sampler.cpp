#include "wallpaper/metrics_sampler.h"

namespace wallpaper {

MetricsSampler::MetricsSampler(const std::size_t maxSamples)
    : maxSamples_(maxSamples == 0 ? 1 : maxSamples) {
  samples_.reserve(maxSamples_);
}

void MetricsSampler::PushSample(const RuntimeMetrics sample) {
  if (samples_.size() == maxSamples_) {
    samples_.erase(samples_.begin());
  }
  samples_.push_back(sample);
}

RuntimeMetrics MetricsSampler::Latest() const {
  if (samples_.empty()) {
    return {};
  }
  return samples_.back();
}

std::vector<RuntimeMetrics> MetricsSampler::Snapshot() const { return samples_; }

}  // namespace wallpaper
