#pragma once

#include <array>
#include <cstddef>

#include "wallpaper/frame_bridge.h"

namespace wallpaper {

enum class VideoFrameRoute {
  kExternalGpuNv12 = 0,
  kGpuTextureCopy = 1,
  kCpuNv12Upload = 2,
  kCpuRgbaUpload = 3,
};

struct VideoFrameRoutePlan final {
  std::array<VideoFrameRoute, 4> routes{};
  std::size_t count = 0;
};

inline void AppendVideoFrameRoute(VideoFrameRoutePlan* const plan,
                                  const VideoFrameRoute route) {
  if (plan == nullptr || plan->count >= plan->routes.size()) {
    return;
  }

  plan->routes[plan->count] = route;
  ++plan->count;
}

[[nodiscard]] inline VideoFrameRoutePlan BuildVideoFrameRoutePlan(
    const frame_bridge::LatestFrame& frame) {
  VideoFrameRoutePlan plan;

  const bool hasGpuTexture = frame.gpuTexture != nullptr;
  const bool hasGpuAuxTexture = frame.gpuAuxTexture != nullptr;
  const bool hasCpuNv12Planes = frame.yPlaneData != nullptr && frame.uvPlaneData != nullptr;

  // 这里故意只编码“候选路径顺序”，不混入任何 D3D 资源创建/上传细节。
  if (frame.gpuBacked && frame.pixelFormat == frame_bridge::PixelFormat::kNv12 &&
      hasGpuTexture && hasGpuAuxTexture) {
    AppendVideoFrameRoute(&plan, VideoFrameRoute::kExternalGpuNv12);
    AppendVideoFrameRoute(&plan, VideoFrameRoute::kGpuTextureCopy);
    return plan;
  }

  if (frame.gpuBacked && hasGpuTexture) {
    AppendVideoFrameRoute(&plan, VideoFrameRoute::kGpuTextureCopy);
    return plan;
  }

  if (frame.pixelFormat == frame_bridge::PixelFormat::kNv12 && hasCpuNv12Planes) {
    AppendVideoFrameRoute(&plan, VideoFrameRoute::kCpuNv12Upload);
    return plan;
  }

  if (frame.rgbaData != nullptr) {
    AppendVideoFrameRoute(&plan, VideoFrameRoute::kCpuRgbaUpload);
  }

  return plan;
}

}  // namespace wallpaper
