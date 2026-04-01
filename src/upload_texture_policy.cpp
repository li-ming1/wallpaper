#include "wallpaper/upload_texture_policy.h"

namespace wallpaper {

namespace {

constexpr std::size_t kCompactCpuFallbackPixels = 960U * 540U;

}  // namespace

bool ShouldUseDefaultTextureUpload(const DecodePath decodePath,
                                   const std::size_t decodeOutputPixels,
                                   const bool scaledUpload) noexcept {
  return IsCpuFallbackDecodePath(decodePath) && !scaledUpload && decodeOutputPixels > 0 &&
         decodeOutputPixels > kCompactCpuFallbackPixels;
}

bool ShouldUseSharedGpuNv12Bridge(const DecodePath decodePath,
                                  const std::size_t decodeOutputPixels,
                                  const bool scaledUpload) noexcept {
  return decodePath == DecodePath::kCpuNv12Fallback && !scaledUpload && decodeOutputPixels > 0 &&
         decodeOutputPixels > kCompactCpuFallbackPixels;
}

}  // namespace wallpaper
