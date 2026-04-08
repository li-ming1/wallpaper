#include "wallpaper/cpu_frame_downscale.h"
#include "wallpaper/cpu_frame_buffer_pool.h"

#include <array>

#include "test_support.h"

TEST_CASE(CpuFrameDownscale_DownscalesRgbaNearestToCompactBuffer) {
  const std::array<std::uint8_t, 64> src = {
      1, 0, 0, 255, 2, 0, 0, 255, 3, 0, 0, 255, 4, 0, 0, 255,
      5, 0, 0, 255, 6, 0, 0, 255, 7, 0, 0, 255, 8, 0, 0, 255,
      9, 0, 0, 255, 10, 0, 0, 255, 11, 0, 0, 255, 12, 0, 0, 255,
      13, 0, 0, 255, 14, 0, 0, 255, 15, 0, 0, 255, 16, 0, 0, 255,
  };

  wallpaper::CpuFrameBufferPool pool(2);
  wallpaper::CompactCpuFrameBuffer scaled;
  const bool ok = wallpaper::TryDownscaleRgbaFrameNearest(
      src.data(), 4, 4, 16, 2, 2, &pool, &scaled);

  EXPECT_TRUE(ok);
  EXPECT_EQ(scaled.width, 2);
  EXPECT_EQ(scaled.height, 2);
  EXPECT_EQ(scaled.primaryStrideBytes, 8);
  EXPECT_EQ(scaled.secondaryStrideBytes, 0);
  EXPECT_EQ(scaled.primaryPlaneOffsetBytes, 0U);
  EXPECT_EQ(scaled.secondaryPlaneOffsetBytes, 0U);
  EXPECT_EQ(scaled.dataBytes, 16U);
  EXPECT_TRUE(scaled.data != nullptr);
  EXPECT_TRUE(scaled.holder != nullptr);
  EXPECT_EQ(scaled.data[0], 1U);
  EXPECT_EQ(scaled.data[4], 3U);
  EXPECT_EQ(scaled.data[8], 9U);
  EXPECT_EQ(scaled.data[12], 11U);
}

TEST_CASE(CpuFrameDownscale_DownscalesNv12NearestToCompactBuffer) {
  const std::array<std::uint8_t, 24> src = {
      0, 1, 2, 3,
      4, 5, 6, 7,
      8, 9, 10, 11,
      12, 13, 14, 15,
      100, 101, 102, 103,
      104, 105, 106, 107,
  };

  wallpaper::CpuFrameBufferPool pool(2);
  wallpaper::CompactCpuFrameBuffer scaled;
  const bool ok = wallpaper::TryDownscaleNv12FrameNearest(
      src.data(), 4, src.data() + 16, 4, 4, 4, 2, 2, &pool, &scaled);

  EXPECT_TRUE(ok);
  EXPECT_EQ(scaled.width, 2);
  EXPECT_EQ(scaled.height, 2);
  EXPECT_EQ(scaled.primaryStrideBytes, 2);
  EXPECT_EQ(scaled.secondaryStrideBytes, 2);
  EXPECT_EQ(scaled.primaryPlaneOffsetBytes, 0U);
  EXPECT_EQ(scaled.secondaryPlaneOffsetBytes, 4U);
  EXPECT_EQ(scaled.dataBytes, 6U);
  EXPECT_TRUE(scaled.data != nullptr);
  EXPECT_TRUE(scaled.holder != nullptr);
  EXPECT_EQ(scaled.data[0], 0U);
  EXPECT_EQ(scaled.data[1], 2U);
  EXPECT_EQ(scaled.data[2], 8U);
  EXPECT_EQ(scaled.data[3], 10U);
  EXPECT_EQ(scaled.data[4], 100U);
  EXPECT_EQ(scaled.data[5], 101U);
}

TEST_CASE(CpuFrameDownscale_RejectsInvalidInput) {
  wallpaper::CpuFrameBufferPool pool(1);
  wallpaper::CompactCpuFrameBuffer scaled;
  EXPECT_TRUE(!wallpaper::TryDownscaleRgbaFrameNearest(nullptr, 4, 4, 16, 2, 2, &pool, &scaled));
  EXPECT_TRUE(!wallpaper::TryDownscaleNv12FrameNearest(
      nullptr, 0, nullptr, 0, 4, 4, 2, 2, &pool, &scaled));
}
