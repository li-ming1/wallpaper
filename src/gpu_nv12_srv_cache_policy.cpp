#include "wallpaper/gpu_nv12_srv_cache_policy.h"

namespace wallpaper {

bool ShouldResetGpuNv12SrvCache(const GpuNv12SrvCacheState& state,
                                const void* const sourceTexture,
                                const std::uint32_t width,
                                const std::uint32_t height) noexcept {
  if (state.sourceTexture != sourceTexture) {
    return true;
  }
  if (state.width != width || state.height != height) {
    return true;
  }
  return false;
}

}  // namespace wallpaper

