#include "wallpaper/video_present_policy.h"

namespace wallpaper {

bool ShouldSkipRedundantVideoPresent(const bool hasNewVideoFrame,
                                     const bool backBufferVideoReady) noexcept {
  if (hasNewVideoFrame) {
    return false;
  }
  return backBufferVideoReady;
}

}  // namespace wallpaper
