#include "wallpaper/quality_governor.h"

namespace wallpaper {
namespace {

constexpr int kRecoverStableSamples = 3;

}  // namespace

QualityGovernor::QualityGovernor() = default;

void QualityGovernor::SetEnabled(const bool enabled) noexcept {
  enabled_ = enabled;
  if (!enabled_) {
    currentFps_ = targetFps_;
    stableSamples_ = 0;
  }
}

bool QualityGovernor::IsEnabled() const noexcept { return enabled_; }

void QualityGovernor::SetTargetFps(const int fpsCap) noexcept {
  targetFps_ = NormalizeFpsCap(fpsCap);
  if (currentFps_ > targetFps_) {
    currentFps_ = targetFps_;
  }
  if (targetFps_ <= 30) {
    currentFps_ = 30;
    stableSamples_ = 0;
  }
}

int QualityGovernor::TargetFps() const noexcept { return targetFps_; }

int QualityGovernor::CurrentFps() const noexcept { return currentFps_; }

int QualityGovernor::Update(const RuntimeMetrics& metrics) noexcept {
  if (!enabled_) {
    currentFps_ = targetFps_;
    stableSamples_ = 0;
    return currentFps_;
  }
  if (targetFps_ <= 30) {
    currentFps_ = 30;
    stableSamples_ = 0;
    return currentFps_;
  }

  if (IsHighLoad(metrics)) {
    currentFps_ = 30;
    stableSamples_ = 0;
    return currentFps_;
  }

  if (currentFps_ < targetFps_ && IsStableLoad(metrics)) {
    ++stableSamples_;
    if (stableSamples_ >= kRecoverStableSamples) {
      currentFps_ = targetFps_;
      stableSamples_ = 0;
    }
  } else {
    stableSamples_ = 0;
  }

  return currentFps_;
}

void QualityGovernor::Reset() noexcept { stableSamples_ = 0; }

bool QualityGovernor::IsHighLoad(const RuntimeMetrics& metrics) const noexcept {
  return metrics.cpuPercent >= 7.5 || metrics.presentP95Ms >= 10.0 ||
         metrics.droppedFrameRatio >= 0.02;
}

bool QualityGovernor::IsStableLoad(const RuntimeMetrics& metrics) const noexcept {
  return metrics.cpuPercent <= 4.5 && metrics.presentP95Ms <= 6.0 &&
         metrics.droppedFrameRatio <= 0.005;
}

}  // namespace wallpaper
