#pragma once

#include <chrono>

namespace wallpaper {

// 主循环睡眠策略：在“响应性”和“CPU 空转”之间做上限约束。
[[nodiscard]] int ComputeMainLoopSleepMs(bool shouldPause, bool hasActiveVideo,
                                         std::chrono::milliseconds untilNextRender) noexcept;

// 解码泵退避策略：无帧时逐步拉长等待，有帧时立即恢复低延迟采样。
[[nodiscard]] int ComputeDecodePumpSleepMs(bool decodeReady, bool frameAcquired,
                                           int previousSleepMs) noexcept;

}  // namespace wallpaper
