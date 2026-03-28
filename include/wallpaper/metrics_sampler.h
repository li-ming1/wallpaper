#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace wallpaper {

struct RuntimeMetrics final {
  double cpuPercent = 0.0;
  std::size_t privateBytes = 0;
  std::size_t workingSetBytes = 0;
  double presentP95Ms = 0.0;
  double droppedFrameRatio = 0.0;
};

class MetricsSampler final {
 public:
  explicit MetricsSampler(std::size_t maxSamples);

  void PushSample(RuntimeMetrics sample);
  [[nodiscard]] RuntimeMetrics Latest() const;
  [[nodiscard]] std::vector<RuntimeMetrics> Snapshot() const;

 private:
  std::size_t maxSamples_;
  std::vector<RuntimeMetrics> samples_;
};

}  // namespace wallpaper
