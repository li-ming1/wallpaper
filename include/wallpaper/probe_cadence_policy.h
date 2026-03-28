#pragma once

#include <chrono>

namespace wallpaper {

using ProbeClock = std::chrono::steady_clock;

// 控制高开销系统探测的采样节奏，避免每帧调用 Win32 查询。
[[nodiscard]] bool ShouldRefreshRuntimeProbe(ProbeClock::time_point now,
                                             ProbeClock::time_point lastSampleAt,
                                             std::chrono::milliseconds interval) noexcept;

}  // namespace wallpaper
