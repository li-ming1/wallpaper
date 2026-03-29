#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace wallpaper {

enum class RuntimeThreadQos {
  kNormal = 0,
  kEco = 1,
};

enum class RuntimePowerState {
  kNormal = 0,
  kBatterySaver = 1,
  kDisplayOff = 2,
  kRemoteSession = 3,
  kSessionInactive = 4,
};

struct RuntimeMetrics final {
  std::size_t decodeOutputPixels = 0;
  RuntimeThreadQos threadQos = RuntimeThreadQos::kNormal;
  bool occluded = false;
  RuntimePowerState powerState = RuntimePowerState::kNormal;
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
