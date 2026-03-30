#pragma once

#include <chrono>

namespace wallpaper {

// 仅在确实换源且当前存在壁纸宿主时，才值得主动回收渲染侧视频纹理。
[[nodiscard]] bool ShouldTrimVideoSurfaceCacheOnSourceChange(bool wallpaperAttached,
                                                             bool pathChanged) noexcept;

// 长静态 pause 期间保留最后一帧即可，超过阈值后可以回收渲染侧视频纹理。
[[nodiscard]] bool ShouldTrimVideoSurfaceCacheDuringStaticPause(
    bool keepWallpaperLayer, bool alreadyTrimmed, std::chrono::milliseconds pausedDuration,
    std::chrono::milliseconds trimAfter) noexcept;

}  // namespace wallpaper
