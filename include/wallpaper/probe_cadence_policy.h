#pragma once

#include <chrono>

namespace wallpaper {

using ProbeClock = std::chrono::steady_clock;

struct RuntimeProbeIntervals final {
  std::chrono::milliseconds session{300};
  std::chrono::milliseconds foreground{120};
};

// 控制高开销系统探测的采样节奏，避免每帧调用 Win32 查询。
[[nodiscard]] bool ShouldRefreshRuntimeProbe(ProbeClock::time_point now,
                                             ProbeClock::time_point lastSampleAt,
                                             std::chrono::milliseconds interval) noexcept;

// 在暂停态降低探测频率，进一步减少系统调用开销。
[[nodiscard]] RuntimeProbeIntervals SelectRuntimeProbeIntervals(bool stablePaused) noexcept;

}  // namespace wallpaper
