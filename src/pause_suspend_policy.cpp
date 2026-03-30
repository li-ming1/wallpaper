#include "wallpaper/pause_suspend_policy.h"

namespace wallpaper {

PauseSuspendThresholdProfile DefaultPauseSuspendThresholdProfile() noexcept {
  return PauseSuspendThresholdProfile{};
}

bool ShouldHardSuspendDuringPause(const std::chrono::milliseconds pausedDuration,
                                  const std::chrono::milliseconds threshold) noexcept {
  return pausedDuration >= threshold;
}

bool ShouldTrimDecodeCacheDuringPause(const std::chrono::milliseconds pausedDuration,
                                      const std::chrono::milliseconds threshold) noexcept {
  return pausedDuration >= threshold;
}

std::chrono::milliseconds SelectHardSuspendThreshold(
    const bool allowAggressiveSuspend, const std::chrono::milliseconds aggressiveThreshold,
    const std::chrono::milliseconds conservativeThreshold) noexcept {
  return allowAggressiveSuspend ? aggressiveThreshold : conservativeThreshold;
}

bool ShouldWarmResumeDuringPause(const bool rawPause, const bool hardSuspended) noexcept {
  return !rawPause && hardSuspended;
}

std::chrono::milliseconds ComputeWarmResumeRetryDelay(const int failureCount) noexcept {
  if (failureCount <= 1) {
    return std::chrono::milliseconds(120);
  }
  if (failureCount == 2) {
    return std::chrono::milliseconds(220);
  }
  if (failureCount == 3) {
    return std::chrono::milliseconds(360);
  }
  return std::chrono::milliseconds(500);
}

std::chrono::milliseconds ComputeResumePipelineRetryDelay(const int failureCount) noexcept {
  if (failureCount <= 1) {
    return std::chrono::milliseconds(160);
  }
  if (failureCount == 2) {
    return std::chrono::milliseconds(260);
  }
  if (failureCount == 3) {
    return std::chrono::milliseconds(420);
  }
  if (failureCount == 4) {
    return std::chrono::milliseconds(700);
  }
  return std::chrono::milliseconds(1000);
}

}  // namespace wallpaper
