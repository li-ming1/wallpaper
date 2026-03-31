#include "wallpaper/runtime_trim_policy.h"

namespace wallpaper {

namespace {

constexpr std::size_t kCompactCpuFallbackPixels = 960U * 540U;

}  // namespace

bool ShouldExecuteLongRunDecodeTrim(const bool trimRequested, const bool decodeRunning,
                                    const DecodePath decodePath) noexcept {
  if (!trimRequested) {
    return false;
  }
  if (!decodeRunning) {
    return true;
  }
  return !IsCpuFallbackDecodePath(decodePath);
}

bool ShouldRequestWorkingSetTrim(const bool hasActiveVideo, const DecodePath decodePath,
                                 const std::size_t workingSetBytes,
                                 const int longRunLoadLevel) noexcept {
  if (!hasActiveVideo || !IsCpuFallbackDecodePath(decodePath)) {
    return false;
  }

  std::size_t thresholdBytes = 20U * 1024U * 1024U;
  if (longRunLoadLevel >= 2) {
    thresholdBytes = 16U * 1024U * 1024U;
  } else if (longRunLoadLevel >= 1) {
    thresholdBytes = 18U * 1024U * 1024U;
  }
  return workingSetBytes >= thresholdBytes;
}

std::chrono::milliseconds SelectActiveWorkingSetTrimInterval(const bool hasActiveVideo,
                                                             const DecodePath decodePath,
                                                             const std::size_t decodeOutputPixels) noexcept {
  if (!ShouldUseAggressiveMemoryPriority(hasActiveVideo, decodePath, decodeOutputPixels)) {
    return std::chrono::milliseconds(0);
  }
  return std::chrono::milliseconds(250);
}

bool ShouldUseAggressiveMemoryPriority(const bool hasActiveVideo, const DecodePath decodePath,
                                       const std::size_t decodeOutputPixels) noexcept {
  return hasActiveVideo && IsCpuFallbackDecodePath(decodePath) && decodeOutputPixels != 0 &&
         decodeOutputPixels <= kCompactCpuFallbackPixels;
}

}  // namespace wallpaper
