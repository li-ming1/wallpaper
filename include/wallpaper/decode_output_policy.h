#pragma once

#include <cstddef>
#include <cstdint>

namespace wallpaper {

struct DecodeOutputHint final {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct DecodeOutputOptions final {
  std::uint32_t desktopWidth = 0;
  std::uint32_t desktopHeight = 0;
  bool cpuFallbackPath = false;
  int longRunLoadLevel = 0;
};

// 质量优先路径统一保持桌面分辨率，不再为 CPU fallback 主动下发降分辨率 hint。
[[nodiscard]] DecodeOutputHint SelectDecodeOutputHint(const DecodeOutputOptions& options) noexcept;

// 仅当协商结果异常高于目标 hint 时才建议 software video processing 重试。
[[nodiscard]] bool ShouldRetryDecodeOpenWithVideoProcessing(
    const DecodeOutputOptions& options, std::uint32_t negotiatedWidth,
    std::uint32_t negotiatedHeight) noexcept;

// 仅在 CPU 回退 + software video processing 场景下开启 advanced video processing，
// 用于提升输出尺寸 hint 命中率（MF_MT_FRAME_SIZE）。
[[nodiscard]] bool ShouldEnableAdvancedVideoProcessing(const DecodeOutputOptions& options,
                                                       bool softwareVideoProcessing) noexcept;

// CPU 回退链路触发 software retry 时，优先保留 D3D 互操作尝试，
// 避免因为一次重试直接退化到纯系统内存路径。
[[nodiscard]] bool ShouldPreserveD3DInteropOnVideoProcessingRetry(
    const DecodeOutputOptions& options, bool preferHardwareTransforms) noexcept;

// 当当前轮次声明为“硬件优先”时，是否要求本轮必须成功绑定 D3D interop；
// 若要求绑定但失败，应把该轮视作失败并由上层进入下一轮降级策略。
[[nodiscard]] bool ShouldRequireD3DInteropBinding(const DecodeOutputOptions& options,
                                                  bool preferHardwareTransforms,
                                                  bool requireHardwareTransforms) noexcept;

// D3D interop + advanced processing 组合下禁用 legacy video processing，
// 避免 SourceReader 回退到系统内存样本路径。
[[nodiscard]] bool ShouldUseLegacySourceReaderVideoProcessing(
    bool tryD3DInterop, bool enableAdvancedVideoProcessing) noexcept;

// 根据长期负载与当前输出像素选择下一轮 decode open 档位：
// - CPU fallback 且输出像素仍高于 540p 时，至少进入 1 档（432p hint）；
// - long-run >=2 时进入 2 档（360p hint）；
// - 非 CPU fallback 维持原有两档（0/2）行为。
[[nodiscard]] int SelectDecodeOpenLongRunLevel(int longRunLoadLevel, bool cpuFallbackPath,
                                               std::size_t decodeOutputPixels) noexcept;

// 是否在本轮 decode open 保持硬件变换优先：
// - CPU fallback 在中压档（1）仍保持硬件优先，避免在部分设备上重开失败；
// - 仅在高压档（>=2）关闭硬件偏好，优先保证降档落地；
// - 其余场景保持硬件优先。
[[nodiscard]] bool ShouldPreferHardwareTransformsForDecodeOpen(int decodeOpenLongRunLevel,
                                                               bool cpuFallbackPath) noexcept;

// 当 CPU fallback 已经通过显式 compact path 压到 540p 级别以下时，不再为 long-run
// 档位变化重开 decode pipeline；此时继续切换 open 档位只会增加回退到 fallback_ticker 的风险。
[[nodiscard]] bool ShouldReopenDecodeForLongRunTuning(bool cpuFallbackPath,
                                                      std::size_t decodeOutputPixels,
                                                      int currentDecodeOpenLevel,
                                                      int desiredDecodeOpenLevel,
                                                      bool currentPreferHardwareTransforms,
                                                      bool desiredPreferHardwareTransforms) noexcept;

}  // namespace wallpaper
