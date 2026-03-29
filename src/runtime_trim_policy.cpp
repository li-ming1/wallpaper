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

}  // namespace wallpaper
