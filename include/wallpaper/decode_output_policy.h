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
// - CPU 回退路径 + 自适应开启：常态限制到 720p，高压长期运行再降到 540p。
[[nodiscard]] DecodeOutputHint SelectDecodeOutputHint(const DecodeOutputOptions& options) noexcept;

}  // namespace wallpaper
