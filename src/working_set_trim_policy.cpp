#include "wallpaper/working_set_trim_policy.h"

namespace wallpaper {

bool ShouldTrimWorkingSetAfterActiveWarmup(
    const bool trimAlreadyExecuted, const bool hasActiveVideo, const bool stablePaused,
    const std::chrono::milliseconds activePlaybackElapsed,
    const std::chrono::milliseconds warmupThreshold) noexcept {
  if (trimAlreadyExecuted || !hasActiveVideo || stablePaused) {
    return false;
  }
  return activePlaybackElapsed >= warmupThreshold;
}

}  // namespace wallpaper
