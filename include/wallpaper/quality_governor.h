#pragma once

#include "wallpaper/config.h"
#include "wallpaper/runtime_metrics.h"

namespace wallpaper {

class QualityGovernor final {
 public:
  QualityGovernor();

  void SetEnabled(bool enabled) noexcept;
  [[nodiscard]] bool IsEnabled() const noexcept;

  void SetTargetFps(int fpsCap) noexcept;
  [[nodiscard]] int TargetFps() const noexcept;
  [[nodiscard]] int CurrentFps() const noexcept;

  [[nodiscard]] int Update(const RuntimeMetrics& metrics) noexcept;
  void Reset() noexcept;

 private:
  [[nodiscard]] bool IsHighLoad(const RuntimeMetrics& metrics) const noexcept;
  [[nodiscard]] bool IsStableLoad(const RuntimeMetrics& metrics) const noexcept;

  bool enabled_ = true;
  int targetFps_ = 30;
  int currentFps_ = 30;
  int stableSamples_ = 0;
};

}  // namespace wallpaper
