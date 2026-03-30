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

  const double maxPixels =
      options.longRunLoadLevel >= 2 ? (960.0 * 540.0) : (1280.0 * 720.0);
  const double currentPixels =
      static_cast<double>(options.desktopWidth) * static_cast<double>(options.desktopHeight);
  constexpr double kNativeCpuFloorPixels = 1280.0 * 720.0;
  if (currentPixels <= kNativeCpuFloorPixels) {
    return hint;
  }
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

bool ShouldRetryDecodeOpenWithVideoProcessing(const DecodeOutputOptions& options,
                                              const std::uint32_t negotiatedWidth,
                                              const std::uint32_t negotiatedHeight) noexcept {
  if (!options.adaptiveQualityEnabled || !options.cpuFallbackPath || negotiatedWidth == 0 ||
      negotiatedHeight == 0) {
    return false;
  }
  const DecodeOutputHint hint = SelectDecodeOutputHint(options);
  if (hint.width == 0 || hint.height == 0) {
    return false;
  }
  return negotiatedWidth > hint.width || negotiatedHeight > hint.height;
}

bool ShouldEnableAdvancedVideoProcessing(const DecodeOutputOptions& options,
                                         const bool softwareVideoProcessing) noexcept {
  if (!softwareVideoProcessing || !options.adaptiveQualityEnabled || !options.cpuFallbackPath) {
    return false;
  }
  return options.desktopWidth > 0 && options.desktopHeight > 0;
}

}  // namespace wallpaper
