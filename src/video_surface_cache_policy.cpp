#include "wallpaper/video_surface_cache_policy.h"

namespace wallpaper {

bool ShouldTrimVideoSurfaceCacheOnSourceChange(const bool wallpaperAttached,
                                               const bool pathChanged) noexcept {
  return wallpaperAttached && pathChanged;
}

bool ShouldTrimVideoSurfaceCacheDuringStaticPause(
    const bool keepWallpaperLayer, const bool alreadyTrimmed,
    const std::chrono::milliseconds pausedDuration,
    const std::chrono::milliseconds trimAfter) noexcept {
  if (!keepWallpaperLayer || alreadyTrimmed) {
    return false;
  }
  return pausedDuration >= trimAfter;
}

}  // namespace wallpaper
