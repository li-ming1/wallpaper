#pragma once

#include <cstdint>

namespace wallpaper {

struct DecodeOutputHint final {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// 选择 SourceReader 输出尺寸提示：
// - 自适应关闭：保持桌面分辨率；
// - GPU 零拷贝路径：保持桌面分辨率；
// - CPU 回退路径 + 自适应开启：限制像素总量，降低解码与上传开销。
[[nodiscard]] DecodeOutputHint SelectDecodeOutputHint(std::uint32_t desktopWidth,
                                                      std::uint32_t desktopHeight,
                                                      bool adaptiveQualityEnabled,
                                                      bool cpuFallbackPath) noexcept;

}  // namespace wallpaper
