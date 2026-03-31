#include "wallpaper/runtime_trim_policy.h"

namespace wallpaper {

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

}  // namespace wallpaper
