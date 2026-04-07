#include "wallpaper/video_frame_route_policy.h"

#include "test_support.h"

TEST_CASE(VideoFrameRoutePolicy_PrefersExternalNv12GpuBridgeFirst) {
  wallpaper::frame_bridge::LatestFrame frame;
  frame.gpuBacked = true;
  frame.pixelFormat = wallpaper::frame_bridge::PixelFormat::kNv12;
  frame.gpuTexture = reinterpret_cast<ID3D11Texture2D*>(0x1);
  frame.gpuAuxTexture = reinterpret_cast<ID3D11Texture2D*>(0x2);

  const auto plan = wallpaper::BuildVideoFrameRoutePlan(frame);

  EXPECT_EQ(plan.count, static_cast<std::size_t>(2));
  EXPECT_EQ(static_cast<int>(plan.routes[0]),
            static_cast<int>(wallpaper::VideoFrameRoute::kExternalGpuNv12));
  EXPECT_EQ(static_cast<int>(plan.routes[1]),
            static_cast<int>(wallpaper::VideoFrameRoute::kGpuTextureCopy));
}

TEST_CASE(VideoFrameRoutePolicy_FallsBackToCpuNv12WhenNoGpuResourcesExist) {
  wallpaper::frame_bridge::LatestFrame frame;
  frame.pixelFormat = wallpaper::frame_bridge::PixelFormat::kNv12;
  frame.yPlaneData = reinterpret_cast<const std::uint8_t*>(0x1);
  frame.uvPlaneData = reinterpret_cast<const std::uint8_t*>(0x2);

  const auto plan = wallpaper::BuildVideoFrameRoutePlan(frame);

  EXPECT_EQ(plan.count, static_cast<std::size_t>(1));
  EXPECT_EQ(static_cast<int>(plan.routes[0]),
            static_cast<int>(wallpaper::VideoFrameRoute::kCpuNv12Upload));
}

TEST_CASE(VideoFrameRoutePolicy_UsesCpuRgbaAsLastAvailablePath) {
  wallpaper::frame_bridge::LatestFrame frame;
  frame.rgbaData = reinterpret_cast<const std::uint8_t*>(0x1);

  const auto plan = wallpaper::BuildVideoFrameRoutePlan(frame);

  EXPECT_EQ(plan.count, static_cast<std::size_t>(1));
  EXPECT_EQ(static_cast<int>(plan.routes[0]),
            static_cast<int>(wallpaper::VideoFrameRoute::kCpuRgbaUpload));
}
