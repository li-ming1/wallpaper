#include "wallpaper/loop_sleep_policy.h"

#include <algorithm>

namespace wallpaper {

int ComputeMainLoopSleepMs(const bool shouldPause, const bool hasActiveVideo,
                           const std::chrono::milliseconds untilNextRender) noexcept {
  if (shouldPause) {
    return 110;
  }
  if (!hasActiveVideo) {
    return 70;
  }

  const auto waitMs = static_cast<int>(untilNextRender.count());
  return std::clamp(waitMs, 2, 24);
}

int ComputeDecodePumpSleepMs(const bool decodeReady, const bool frameAcquired,
                             const int previousSleepMs) noexcept {
  if (!decodeReady) {
    return 70;
  }
  if (frameAcquired) {
    return 2;
  }
  return std::clamp(previousSleepMs + 1, 2, 16);
}

int ComputeDecodePumpHotSleepMs(const int renderFpsCap) noexcept {
  return renderFpsCap >= 60 ? 8 : 16;
}

bool ShouldUseHighResolutionTimer(const bool hasActiveVideo, const bool stablePaused,
                                  const int appliedFpsCap,
                                  const int longRunLoadLevel) noexcept {
  if (!hasActiveVideo || stablePaused) {
    return false;
  }
  if (appliedFpsCap < 60) {
    return false;
  }
  return longRunLoadLevel <= 0;
}

}  // namespace wallpaper
