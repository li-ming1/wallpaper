#pragma once

#include <chrono>

#include "wallpaper/interfaces.h"

namespace wallpaper {

// 主循环睡眠策略：在“响应性”和“CPU 空转”之间做上限约束。
[[nodiscard]] int ComputeMainLoopSleepMs(bool shouldPause, bool hasActiveVideo,
                                         std::chrono::milliseconds untilNextRender) noexcept;

// 动态桌面渲染期间优先固定睡眠，避免输入消息导致的额外提前唤醒。
[[nodiscard]] bool ShouldUseMainLoopMessageAwareWait(bool shouldPause,
                                                     bool hasActiveVideo) noexcept;

// 解码泵退避策略：无帧时逐步拉长等待，有帧时立即恢复低延迟采样。
[[nodiscard]] int ComputeDecodePumpSleepMs(bool decodeReady, bool frameAcquired,
                                           int previousSleepMs,
                                           bool frameReadyNotifierAvailable) noexcept;

// 根据当前渲染帧率上限给出“有帧时”解码泵睡眠，避免长期动态场景忙轮询。
[[nodiscard]] int ComputeDecodePumpHotSleepMs(int renderFpsCap, int sourceFps) noexcept;

// 为了解码速度不低于素材 1x，限制 hot-sleep 不超过源帧间隔的安全上限。
[[nodiscard]] int ClampDecodePumpHotSleepForRealtime(int requestedHotSleepMs,
                                                     int renderFpsCap,
                                                     int sourceFps) noexcept;

// 渲染参数未变化时避免重复唤醒解码泵，减少无效线程调度。
[[nodiscard]] bool ShouldWakeDecodePumpForRenderCapUpdate(int previousHotSleepMs,
                                                          int nextHotSleepMs,
                                                          int previousFpsCap,
                                                          int nextFpsCap) noexcept;

// 唤醒标记已置位时跳过重复通知，避免多次无效 condition_variable 唤醒。
[[nodiscard]] bool ShouldNotifyDecodePumpWake(bool wakeAlreadyRequested) noexcept;

// notifier 可用时优先事件驱动等待，减少“有帧也周期轮询”的无效唤醒。
[[nodiscard]] bool ShouldPreferEventDrivenDecodePumpWait(bool frameReadyNotifierAvailable,
                                                         bool decodeReady,
                                                         bool frameAcquired) noexcept;

// 解码泵可中断等待窗口选择：有帧时缩短窗口提升节拍，无帧时保持长窗口控 CPU。
[[nodiscard]] int SelectDecodePumpInterruptibleWaitMs(int requestedSleepMs,
                                                      bool preferEventDrivenWait,
                                                      bool frameAcquired) noexcept;

// 高精度计时器仅在“桌面动态 + 60fps + 低压力”场景启用，避免长期系统功耗抬升。
[[nodiscard]] bool ShouldUseHighResolutionTimer(bool hasActiveVideo, bool stablePaused,
                                                int appliedFpsCap, int longRunLoadLevel,
                                                DecodePath decodePath,
                                                bool warmupActive) noexcept;

}  // namespace wallpaper
