#pragma once

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
#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L
  [[nodiscard]] auto Capacity(this const MetricsSampler& self) noexcept -> std::size_t {
    return self.maxSamples_;
  }
#else
  [[nodiscard]] std::size_t Capacity() const noexcept { return maxSamples_; }
#endif
  [[nodiscard]] std::size_t Size() const noexcept { return size_; }
  [[nodiscard]] RuntimeMetrics Latest() const;
  [[nodiscard]] RuntimeMetrics SampleAtOldestOffset(std::size_t index) const;

 private:
  std::size_t maxSamples_;
  std::vector<RuntimeMetrics> samples_;
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace wallpaper
