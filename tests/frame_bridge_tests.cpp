#include "wallpaper/frame_bridge.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "test_support.h"

TEST_CASE(FrameBridge_PublishAndReadLatestFrame) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(7)));
  wallpaper::frame_bridge::PublishLatestFrame(2, 2, 8, 1000, 1, pixels);

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrame(&latest));
  EXPECT_EQ(latest.width, 2);
  EXPECT_EQ(latest.height, 2);
  EXPECT_EQ(latest.strideBytes, 8);
  EXPECT_EQ(static_cast<int>(latest.sequence), 1);
  EXPECT_TRUE(latest.rgbaPixels != nullptr);
}

TEST_CASE(FrameBridge_ClearRemovesFrame) {
  auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(4, static_cast<std::uint8_t>(1)));
  wallpaper::frame_bridge::PublishLatestFrame(1, 1, 4, 100, 3, pixels);
  wallpaper::frame_bridge::ClearLatestFrame();

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(!wallpaper::frame_bridge::TryGetLatestFrame(&latest));
}

TEST_CASE(FrameBridge_PublishAndReadLatestGpuFrame) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto textureHolder = std::shared_ptr<void>(reinterpret_cast<void*>(0x1), [](void*) {});
  wallpaper::frame_bridge::PublishLatestGpuFrame(1920, 1080, 1000, 7, 87, 0,
                                                 std::move(textureHolder));

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrame(&latest));
  EXPECT_EQ(latest.width, 1920);
  EXPECT_EQ(latest.height, 1080);
  EXPECT_TRUE(latest.gpuBacked);
  EXPECT_TRUE(latest.gpuTexture != nullptr);
  EXPECT_EQ(static_cast<int>(latest.sequence), 7);
  EXPECT_TRUE(latest.rgbaPixels == nullptr);
}

TEST_CASE(FrameBridge_PublishAndReadLatestNv12Frame) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto nv12Holder = std::make_shared<std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(12, static_cast<std::uint8_t>(9)));
  wallpaper::frame_bridge::PublishLatestNv12FrameView(
      4, 2, 4, 4, 1000, 9, nv12Holder->data(), 8, nv12Holder->data() + 8, 4,
      std::shared_ptr<void>(nv12Holder, nv12Holder->data()));

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrame(&latest));
  EXPECT_EQ(latest.width, 4);
  EXPECT_EQ(latest.height, 2);
  EXPECT_EQ(static_cast<int>(latest.sequence), 9);
  EXPECT_EQ(static_cast<int>(latest.pixelFormat),
            static_cast<int>(wallpaper::frame_bridge::PixelFormat::kNv12));
  EXPECT_TRUE(latest.yPlaneData != nullptr);
  EXPECT_TRUE(latest.uvPlaneData != nullptr);
  EXPECT_EQ(latest.yPlaneBytes, static_cast<std::size_t>(8));
  EXPECT_EQ(latest.uvPlaneBytes, static_cast<std::size_t>(4));
}
