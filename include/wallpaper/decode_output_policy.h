#pragma once

#include <cstdint>

namespace wallpaper {

struct DecodeOutputHint final {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct DecodeOutputOptions final {
  std::uint32_t desktopWidth = 0;
  std::uint32_t desktopHeight = 0;
  bool adaptiveQualityEnabled = true;
  bool cpuFallbackPath = false;
  int longRunLoadLevel = 0;
};

// 选择 SourceReader 输出尺寸提示：
// - 自适应关闭：保持桌面分辨率；
// - GPU 零拷贝路径：保持桌面分辨率；
// - CPU 回退路径 + 自适应开启：常态限制到 540p，中压降到 432p，高压降到 360p。
[[nodiscard]] DecodeOutputHint SelectDecodeOutputHint(const DecodeOutputOptions& options) noexcept;

// 当 CPU 回退路径在自适应开启时协商结果高于目标 hint，建议切到 video processing 重试一次。
[[nodiscard]] bool ShouldRetryDecodeOpenWithVideoProcessing(
    const DecodeOutputOptions& options, std::uint32_t negotiatedWidth,
    std::uint32_t negotiatedHeight) noexcept;

// 仅在 CPU 回退 + 自适应质量场景下开启 advanced video processing，
// 用于提升输出尺寸 hint 命中率（MF_MT_FRAME_SIZE）。
[[nodiscard]] bool ShouldEnableAdvancedVideoProcessing(const DecodeOutputOptions& options,
                                                       bool softwareVideoProcessing) noexcept;

// 自适应 CPU 回退链路触发 software retry 时，优先保留 D3D 互操作尝试，
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

}  // namespace wallpaper
