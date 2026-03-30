#include "wallpaper/decode_output_policy.h"

#include <algorithm>
#include <cmath>

namespace wallpaper {

DecodeOutputHint SelectDecodeOutputHint(const DecodeOutputOptions& options) noexcept {
  DecodeOutputHint hint{options.desktopWidth, options.desktopHeight};
  if (options.desktopWidth == 0 || options.desktopHeight == 0) {
    return {};
  }
  if (!options.adaptiveQualityEnabled || !options.cpuFallbackPath) {
    return hint;
  }

  if (options.longRunLoadLevel < 2) {
    return hint;
  }

  constexpr double maxPixels = 1280.0 * 720.0;
  const double currentPixels =
      static_cast<double>(options.desktopWidth) * static_cast<double>(options.desktopHeight);
  if (currentPixels <= maxPixels) {
    return hint;
  }

  const double scale = std::sqrt(maxPixels / currentPixels);
  std::uint32_t scaledWidth = static_cast<std::uint32_t>(
      std::max(2.0, std::floor(static_cast<double>(options.desktopWidth) * scale)));
  std::uint32_t scaledHeight = static_cast<std::uint32_t>(
      std::max(2.0, std::floor(static_cast<double>(options.desktopHeight) * scale)));

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
