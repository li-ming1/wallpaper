#pragma once

#include <chrono>
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
// - long-run 越高，触发阈值越低（0:20MB, 1:18MB, >=2:16MB）。
[[nodiscard]] bool ShouldRequestWorkingSetTrim(bool hasActiveVideo, DecodePath decodePath,
                                               std::size_t workingSetBytes,
                                               int longRunLoadLevel) noexcept;

// 运行态 working-set trim 的节奏选择：
// - 全画质 CPU fallback 降低 trim 频率，尽量避免 `EmptyWorkingSet` 周期性抬高 CPU；
// - 紧凑 CPU fallback 保持较快兜底回收；
// - 非活跃或非 CPU fallback 直接关闭。
[[nodiscard]] std::chrono::milliseconds SelectRuntimeWorkingSetTrimInterval(
    bool hasActiveVideo, DecodePath decodePath, std::size_t decodeOutputPixels,
    int longRunLoadLevel) noexcept;

// 共享 GPU NV12 桥接的全画质 CPU fallback 在首帧后预付一次 working-set trim，
// 目标是把最重的一次回收尽量挪到 warmup，而不是落在活跃采样窗口内。
[[nodiscard]] bool ShouldExecuteStartupWorkingSetTrim(bool hasActiveVideo,
                                                      DecodePath decodePath,
                                                      std::size_t decodeOutputPixels,
                                                      bool gpuBacked,
                                                      bool alreadyTrimmed) noexcept;

// CPU fallback 动态播放统一下调进程内存优先级，帮助系统更积极回收已完成上传/呈现的热页。
[[nodiscard]] bool ShouldUseAggressiveMemoryPriority(bool hasActiveVideo, DecodePath decodePath,
                                                     std::size_t decodeOutputPixels) noexcept;

// CPU fallback 在成功 present 后允许触发 working-set trim：
// - 紧凑档维持高频回收；
// - 全画质档改为低频 + 阈值门控，优先压峰值而不是盲目每帧回收。
[[nodiscard]] bool ShouldTrimWorkingSetImmediatelyAfterPresent(bool hasActiveVideo,
                                                               DecodePath decodePath,
                                                               std::size_t decodeOutputPixels) noexcept;

[[nodiscard]] std::chrono::milliseconds SelectPostPresentWorkingSetTrimInterval(
    bool hasActiveVideo, DecodePath decodePath, std::size_t decodeOutputPixels) noexcept;

}  // namespace wallpaper
