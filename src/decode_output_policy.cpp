#include "wallpaper/decode_output_policy.h"

#include <algorithm>
#include <cmath>

namespace wallpaper {

DecodeOutputHint SelectDecodeOutputHint(const DecodeOutputOptions& options) noexcept {
  DecodeOutputHint hint{options.desktopWidth, options.desktopHeight};
  if (options.desktopWidth == 0 || options.desktopHeight == 0) {
    return {};
  }
  return hint;
}

bool ShouldRetryDecodeOpenWithVideoProcessing(const DecodeOutputOptions& options,
                                              const std::uint32_t negotiatedWidth,
                                              const std::uint32_t negotiatedHeight) noexcept {
  if (!options.cpuFallbackPath || negotiatedWidth == 0 || negotiatedHeight == 0) {
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
  if (!softwareVideoProcessing || !options.cpuFallbackPath) {
    return false;
  }
  return options.desktopWidth > 0 && options.desktopHeight > 0;
}

bool ShouldPreserveD3DInteropOnVideoProcessingRetry(
    const DecodeOutputOptions& options, const bool preferHardwareTransforms) noexcept {
  if (!preferHardwareTransforms) {
    return false;
  }
  if (!options.cpuFallbackPath) {
    return false;
  }
  return options.desktopWidth > 0 && options.desktopHeight > 0;
}

bool ShouldRequireD3DInteropBinding(const DecodeOutputOptions& options,
                                    const bool preferHardwareTransforms,
                                    const bool requireHardwareTransforms) noexcept {
  if (requireHardwareTransforms) {
    return true;
  }
  if (!preferHardwareTransforms) {
    return false;
  }
  if (!options.cpuFallbackPath) {
    return false;
  }
  return options.desktopWidth > 0 && options.desktopHeight > 0;
}

bool ShouldUseLegacySourceReaderVideoProcessing(
    const bool tryD3DInterop, const bool enableAdvancedVideoProcessing) noexcept {
  return !(tryD3DInterop && enableAdvancedVideoProcessing);
}

int SelectDecodeOpenLongRunLevel(const int longRunLoadLevel, const bool cpuFallbackPath,
                                 const std::size_t decodeOutputPixels) noexcept {
  if (longRunLoadLevel >= 2) {
    return 2;
  }
  if (!cpuFallbackPath) {
    return 0;
  }

  constexpr std::size_t kCpuFallbackPixelTarget = 960U * 540U;
  if (decodeOutputPixels > kCpuFallbackPixelTarget) {
    return 1;
  }
  return 0;
}

bool ShouldPreferHardwareTransformsForDecodeOpen(const int decodeOpenLongRunLevel,
                                                 const bool cpuFallbackPath) noexcept {
  if (!cpuFallbackPath) {
    return true;
  }
  return decodeOpenLongRunLevel <= 1;
}

bool ShouldReopenDecodeForLongRunTuning(const bool cpuFallbackPath,
                                        const std::size_t decodeOutputPixels,
                                        const int currentDecodeOpenLevel,
                                        const int desiredDecodeOpenLevel,
                                        const bool currentPreferHardwareTransforms,
                                        const bool desiredPreferHardwareTransforms) noexcept {
  constexpr std::size_t kCompactCpuFallbackPixels = 960U * 540U;
  const bool tuningChanged = currentDecodeOpenLevel != desiredDecodeOpenLevel ||
                             currentPreferHardwareTransforms != desiredPreferHardwareTransforms;
  if (!tuningChanged) {
    return false;
  }
  if (cpuFallbackPath && decodeOutputPixels != 0 &&
      decodeOutputPixels <= kCompactCpuFallbackPixels) {
    return false;
  }
  return true;
}

}  // namespace wallpaper
