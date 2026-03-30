#pragma once

#include <cstddef>

#include "wallpaper/interfaces.h"

namespace wallpaper {

// 主循环长时压力 trim 的执行策略：
// - 未请求时直接跳过；
// - 解码线程未运行时允许执行（用于安全回收窗口）；
// - CPU 回退动态路径禁止执行，避免清帧/flush 带来可见闪烁与顿挫；
// - GPU 零拷贝动态路径允许执行。
[[nodiscard]] bool ShouldExecuteLongRunDecodeTrim(bool trimRequested, bool decodeRunning,
                                                  DecodePath decodePath) noexcept;

// 运行态 working-set 回收门控：
// - 仅在活跃视频 + CPU 回退路径下考虑；
// - long-run 越高，触发阈值越低（0:64MB, 1:40MB, >=2:32MB）。
[[nodiscard]] bool ShouldRequestWorkingSetTrim(bool hasActiveVideo, DecodePath decodePath,
                                               std::size_t workingSetBytes,
                                               int longRunLoadLevel) noexcept;

}  // namespace wallpaper
