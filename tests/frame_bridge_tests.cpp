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
