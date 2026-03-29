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
  return std::clamp(waitMs, 2, 40);
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

int ComputeDecodePumpHotSleepMs(const int renderFpsCap, const int sourceFps) noexcept {
  const int effectiveSourceFps = sourceFps > 0 ? sourceFps : renderFpsCap;
  if (effectiveSourceFps >= 60 && renderFpsCap >= 60) {
    return 14;
  }
  if (effectiveSourceFps >= 30) {
    return 28;
  }
  if (effectiveSourceFps >= 25) {
    return 34;
  }
  if (effectiveSourceFps >= 24) {
    return 36;
  }
  return 40;
}

bool ShouldUseHighResolutionTimer(const bool hasActiveVideo, const bool stablePaused,
                                  const int appliedFpsCap, const int longRunLoadLevel,
                                  const DecodePath decodePath,
                                  const bool warmupActive) noexcept {
  if (!hasActiveVideo || stablePaused) {
    return false;
  }
  if (appliedFpsCap < 60) {
    return false;
  }
  if (longRunLoadLevel > 0) {
    return false;
  }
  return !IsCpuFallbackDecodePath(decodePath) || warmupActive;
}

}  // namespace wallpaper
