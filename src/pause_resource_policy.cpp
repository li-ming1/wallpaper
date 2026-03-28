#include "wallpaper/pause_resource_policy.h"

namespace wallpaper {

bool ShouldReleaseResourcesOnPause(const bool shouldPause,
                                   const bool hasActiveVideoResources) noexcept {
  return shouldPause && hasActiveVideoResources;
}

bool ShouldRestoreResourcesOnResume(const bool isStillPaused,
                                    const bool releasedByPause) noexcept {
  return !isStillPaused && releasedByPause;
}

bool ShouldKeepWallpaperLayerDuringPause(const bool wallpaperAttached,
                                         const bool hasLastPresentedFrame) noexcept {
  return wallpaperAttached && hasLastPresentedFrame;
}

}  // namespace wallpaper
