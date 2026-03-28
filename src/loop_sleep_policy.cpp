#include "wallpaper/loop_sleep_policy.h"

#include <algorithm>

namespace wallpaper {

int ComputeMainLoopSleepMs(const bool shouldPause, const bool hasActiveVideo,
                           const std::chrono::milliseconds untilNextRender) noexcept {
  if (shouldPause) {
    return 30;
  }
  if (!hasActiveVideo) {
    return 50;
  }

  const auto waitMs = static_cast<int>(untilNextRender.count());
  return std::clamp(waitMs, 1, 8);
}

int ComputeDecodePumpSleepMs(const bool decodeReady, const bool frameAcquired,
                             const int previousSleepMs) noexcept {
  if (!decodeReady) {
    return 12;
  }
  if (frameAcquired) {
    return 0;
  }
  return std::clamp(previousSleepMs + 1, 1, 8);
}

}  // namespace wallpaper
