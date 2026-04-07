#pragma once

#include <cstdint>

namespace wallpaper {

struct SharedDeviceCacheDecision final {
  bool refreshRequired = false;
  bool releaseBridgeTextures = false;
};

[[nodiscard]] constexpr SharedDeviceCacheDecision ResolveSharedDeviceCacheDecision(
    const bool hasCachedDevice, const bool hasCachedContext, const std::uint64_t cachedRevision,
    const std::uint64_t currentRevision) noexcept {
  SharedDeviceCacheDecision decision;
  if (!hasCachedDevice || !hasCachedContext) {
    decision.refreshRequired = true;
    return decision;
  }
  if (cachedRevision != currentRevision) {
    decision.refreshRequired = true;
    decision.releaseBridgeTextures = true;
  }
  return decision;
}

}  // namespace wallpaper
