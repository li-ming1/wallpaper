#include "wallpaper/pause_suspend_policy.h"

namespace wallpaper {

bool ShouldHardSuspendDuringPause(const std::chrono::milliseconds pausedDuration,
                                  const std::chrono::milliseconds threshold) noexcept {
  return pausedDuration >= threshold;
}

}  // namespace wallpaper
