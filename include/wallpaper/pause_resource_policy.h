#pragma once

namespace wallpaper {

// 全屏暂停时，仅在当前有活动视频资源时执行释放，避免无意义操作。
[[nodiscard]] bool ShouldReleaseResourcesOnPause(bool shouldPause,
                                                 bool hasActiveVideoResources) noexcept;

// 从暂停恢复时，仅在此前确实执行过资源释放时才做恢复。
[[nodiscard]] bool ShouldRestoreResourcesOnResume(bool isStillPaused,
                                                  bool releasedByPause) noexcept;

}  // namespace wallpaper
