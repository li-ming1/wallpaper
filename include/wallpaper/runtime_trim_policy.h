#pragma once

#include "wallpaper/interfaces.h"

namespace wallpaper {

// 主循环长时压力 trim 的执行策略：
// - 未请求时直接跳过；
// - 解码线程未运行时允许执行（用于安全回收窗口）；
// - CPU 回退动态路径禁止执行，避免清帧/flush 带来可见闪烁与顿挫；
// - GPU 零拷贝动态路径允许执行。
[[nodiscard]] bool ShouldExecuteLongRunDecodeTrim(bool trimRequested, bool decodeRunning,
                                                  DecodePath decodePath) noexcept;

}  // namespace wallpaper
