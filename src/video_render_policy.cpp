#include "wallpaper/video_render_policy.h"

namespace wallpaper {

bool ShouldUseHighQualityUpscale(const int sourceWidth, const int sourceHeight,
                                 const int targetWidth, const int targetHeight) noexcept {
  if (sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0) {
    return false;
  }
  return targetWidth > sourceWidth || targetHeight > sourceHeight;
}

VideoColorSpace SelectVideoColorSpace(const int frameWidth, const int frameHeight) noexcept {
  if (frameWidth >= 1280 || frameHeight >= 720) {
    return VideoColorSpace::kBt709Limited;
  }
  return VideoColorSpace::kBt601Limited;
}

}  // namespace wallpaper
