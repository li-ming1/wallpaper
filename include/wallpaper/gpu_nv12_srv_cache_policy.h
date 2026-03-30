#pragma once

#include <cstdint>

namespace wallpaper {

struct GpuNv12SrvCacheState final {
  const void* sourceTexture = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

[[nodiscard]] bool ShouldResetGpuNv12SrvCache(const GpuNv12SrvCacheState& state,
                                              const void* sourceTexture,
                                              std::uint32_t width,
                                              std::uint32_t height) noexcept;

}  // namespace wallpaper

