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
  EXPECT_TRUE(latest.rgbaDataHolder != nullptr);
  EXPECT_TRUE(latest.rgbaData != nullptr);
  EXPECT_EQ(latest.rgbaDataBytes, static_cast<std::size_t>(16));
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
  EXPECT_TRUE(latest.rgbaData == nullptr);
  EXPECT_TRUE(latest.rgbaDataHolder == nullptr);
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

TEST_CASE(FrameBridge_PublishAndReadLatestGpuNv12Frame) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto yTextureHolder = std::shared_ptr<void>(reinterpret_cast<void*>(0x10), [](void*) {});
  auto uvTextureHolder = std::shared_ptr<void>(reinterpret_cast<void*>(0x20), [](void*) {});
  wallpaper::frame_bridge::PublishLatestGpuNv12Frame(
      1920, 1080, 3000, 13, std::move(yTextureHolder), std::move(uvTextureHolder));

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrame(&latest));
  EXPECT_EQ(latest.width, 1920);
  EXPECT_EQ(latest.height, 1080);
  EXPECT_EQ(static_cast<int>(latest.sequence), 13);
  EXPECT_TRUE(latest.gpuBacked);
  EXPECT_EQ(static_cast<int>(latest.pixelFormat),
            static_cast<int>(wallpaper::frame_bridge::PixelFormat::kNv12));
  EXPECT_TRUE(latest.gpuTexture != nullptr);
  EXPECT_TRUE(latest.gpuAuxTexture != nullptr);
  EXPECT_TRUE(latest.gpuTextureHolder != nullptr);
  EXPECT_TRUE(latest.gpuAuxTextureHolder != nullptr);
  EXPECT_TRUE(latest.rgbaData == nullptr);
}

TEST_CASE(FrameBridge_TryGetLatestFrameIfNewerSkipsSameSequence) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(5)));
  wallpaper::frame_bridge::PublishLatestFrame(2, 2, 8, 2000, 11, pixels);

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrameIfNewer(10, &latest));
  EXPECT_EQ(static_cast<int>(latest.sequence), 11);
  EXPECT_TRUE(!wallpaper::frame_bridge::TryGetLatestFrameIfNewer(11, &latest));
}

TEST_CASE(FrameBridge_ReleaseConsumedKeepsNewerFrameIntact) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(3)));
  wallpaper::frame_bridge::PublishLatestFrame(2, 2, 8, 2000, 21, pixels);
  wallpaper::frame_bridge::ReleaseLatestFrameIfSequenceConsumed(20);

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(wallpaper::frame_bridge::TryGetLatestFrame(&latest));
  EXPECT_EQ(static_cast<int>(latest.sequence), 21);
}

TEST_CASE(FrameBridge_ReleaseConsumedDropsMatchedFrame) {
  wallpaper::frame_bridge::ClearLatestFrame();

  auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
      std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(4)));
  wallpaper::frame_bridge::PublishLatestFrame(2, 2, 8, 2000, 31, pixels);
  wallpaper::frame_bridge::ReleaseLatestFrameIfSequenceConsumed(31);

  wallpaper::frame_bridge::LatestFrame latest;
  EXPECT_TRUE(!wallpaper::frame_bridge::TryGetLatestFrame(&latest));
}
