#pragma once

namespace wallpaper {

// 交换链回写的最后一帧已可直接复用时，跳过重复 Present，避免无新帧场景的无效提交。
[[nodiscard]] bool ShouldSkipRedundantVideoPresent(bool hasNewVideoFrame,
                                                   bool backBufferVideoReady) noexcept;

}  // namespace wallpaper
