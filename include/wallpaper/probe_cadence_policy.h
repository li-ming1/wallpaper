#pragma once

#include <chrono>
#include <cstdint>

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

// 维护前台探测失败计数：成功清零，失败累加。
[[nodiscard]] int UpdateForegroundProbeFailureStreak(bool probeSucceeded,
                                                     int previousStreak) noexcept;

// 连续失败达到阈值后，采用保守策略（按非桌面上下文处理）。
[[nodiscard]] bool ShouldUseConservativeDesktopContext(int failureStreak,
                                                       int failureThreshold) noexcept;

// 当前台窗口句柄稳定时，复用最近一次深度探测结果，减少高开销进程查询。
[[nodiscard]] bool ShouldReuseForegroundProbeResult(
    std::uintptr_t currentForegroundWindowHandle, std::uintptr_t lastForegroundWindowHandle,
    ProbeClock::time_point now, ProbeClock::time_point lastDeepProbeAt,
    std::chrono::milliseconds deepProbeReuseInterval) noexcept;

// 正常稳定态适度放宽会话探测节奏，减少系统调用；异常态保持基础节奏以保证恢复及时性。
[[nodiscard]] std::chrono::milliseconds SelectSessionProbeIntervalForState(
    std::chrono::milliseconds baseInterval, bool sessionInteractive, bool batterySaverActive,
    bool remoteSessionActive) noexcept;

}  // namespace wallpaper
