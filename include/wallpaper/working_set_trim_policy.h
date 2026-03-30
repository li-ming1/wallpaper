#pragma once

#include <chrono>

namespace wallpaper {

// 启动稳定后仅做一次 working-set trim，回收首启冷页，避免长期动态场景保持过高常驻工作集。
[[nodiscard]] bool ShouldTrimWorkingSetAfterActiveWarmup(
    bool trimAlreadyExecuted, bool hasActiveVideo, bool stablePaused,
    std::chrono::milliseconds activePlaybackElapsed,
    std::chrono::milliseconds warmupThreshold) noexcept;

}  // namespace wallpaper
