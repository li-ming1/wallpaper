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

std::chrono::milliseconds SelectRuntimeWorkingSetTrimInterval(const bool hasActiveVideo,
                                                              const DecodePath decodePath,
                                                              const std::size_t decodeOutputPixels,
                                                              const int longRunLoadLevel) noexcept {
  if (!hasActiveVideo || !IsCpuFallbackDecodePath(decodePath)) {
    return std::chrono::milliseconds(0);
  }
  if (decodeOutputPixels > kCompactCpuFallbackPixels) {
    if (longRunLoadLevel >= 2) {
      return std::chrono::milliseconds(2500);
    }
    if (longRunLoadLevel >= 1) {
      return std::chrono::milliseconds(4000);
    }
    return std::chrono::milliseconds(6000);
  }
  return std::chrono::milliseconds(2000);
}

bool ShouldExecuteStartupWorkingSetTrim(const bool hasActiveVideo, const DecodePath decodePath,
                                        const std::size_t decodeOutputPixels,
                                        const bool gpuBacked,
                                        const bool alreadyTrimmed) noexcept {
  return hasActiveVideo && !alreadyTrimmed && gpuBacked &&
         decodePath == DecodePath::kCpuNv12Fallback &&
         decodeOutputPixels > kCompactCpuFallbackPixels;
}

std::chrono::milliseconds SelectActiveWorkingSetTrimInterval(const bool hasActiveVideo,
                                                             const DecodePath decodePath,
                                                             const std::size_t decodeOutputPixels) noexcept {
  (void)hasActiveVideo;
  (void)decodePath;
  (void)decodeOutputPixels;
  return std::chrono::milliseconds(0);
}

bool ShouldUseAggressiveMemoryPriority(const bool hasActiveVideo, const DecodePath decodePath,
                                       const std::size_t decodeOutputPixels) noexcept {
  return hasActiveVideo && IsCpuFallbackDecodePath(decodePath) && decodeOutputPixels != 0;
}

bool ShouldTrimWorkingSetImmediatelyAfterPresent(const bool hasActiveVideo,
                                                 const DecodePath decodePath,
                                                 const std::size_t decodeOutputPixels) noexcept {
  return hasActiveVideo && IsCpuFallbackDecodePath(decodePath) && decodeOutputPixels != 0;
}

std::chrono::milliseconds SelectPostPresentWorkingSetTrimInterval(
    const bool hasActiveVideo, const DecodePath decodePath,
    const std::size_t decodeOutputPixels) noexcept {
  if (!ShouldTrimWorkingSetImmediatelyAfterPresent(hasActiveVideo, decodePath,
                                                   decodeOutputPixels)) {
    return std::chrono::milliseconds(0);
  }
  if (decodeOutputPixels <= kCompactCpuFallbackPixels) {
    return std::chrono::milliseconds(150);
  }
  return std::chrono::milliseconds(0);
}

std::size_t SelectPostPresentWorkingSetTrimThresholdBytes(const bool hasActiveVideo,
                                                          const DecodePath decodePath,
                                                          const std::size_t decodeOutputPixels) noexcept {
  if (!ShouldTrimWorkingSetImmediatelyAfterPresent(hasActiveVideo, decodePath,
                                                   decodeOutputPixels)) {
    return 0;
  }
  if (decodeOutputPixels <= kCompactCpuFallbackPixels) {
    return 0;
  }
  return 0;
}

}  // namespace wallpaper
