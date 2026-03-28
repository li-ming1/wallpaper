#include "wallpaper/decode_output_policy.h"

#include <algorithm>
#include <cmath>

namespace wallpaper {

DecodeOutputHint SelectDecodeOutputHint(const std::uint32_t desktopWidth,
                                        const std::uint32_t desktopHeight,
                                        const bool adaptiveQualityEnabled,
                                        const bool cpuFallbackPath) noexcept {
  DecodeOutputHint hint{desktopWidth, desktopHeight};
  if (desktopWidth == 0 || desktopHeight == 0) {
    return {};
  }
  if (!adaptiveQualityEnabled || !cpuFallbackPath) {
    return hint;
  }

  constexpr double kMaxAdaptiveCpuPixels = 1280.0 * 720.0;
  const double currentPixels =
      static_cast<double>(desktopWidth) * static_cast<double>(desktopHeight);
  if (currentPixels <= kMaxAdaptiveCpuPixels) {
    return hint;
  }

  const double scale = std::sqrt(kMaxAdaptiveCpuPixels / currentPixels);
  std::uint32_t scaledWidth = static_cast<std::uint32_t>(
      std::max(2.0, std::floor(static_cast<double>(desktopWidth) * scale)));
  std::uint32_t scaledHeight = static_cast<std::uint32_t>(
      std::max(2.0, std::floor(static_cast<double>(desktopHeight) * scale)));

  // 偶数尺寸更贴近常见 YUV/RGB 处理链路，减少内部对齐与协商失败概率。
  if ((scaledWidth & 1U) != 0U) {
    --scaledWidth;
  }
  if ((scaledHeight & 1U) != 0U) {
    --scaledHeight;
  }

  hint.width = std::max<std::uint32_t>(2U, scaledWidth);
  hint.height = std::max<std::uint32_t>(2U, scaledHeight);
  return hint;
}

}  // namespace wallpaper
