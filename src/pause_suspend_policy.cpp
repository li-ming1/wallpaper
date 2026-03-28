#include "wallpaper/pause_suspend_policy.h"

namespace wallpaper {

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

}  // namespace wallpaper
