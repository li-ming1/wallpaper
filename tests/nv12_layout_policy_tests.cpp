#include "wallpaper/nv12_layout_policy.h"

#include "test_support.h"

TEST_CASE(Nv12LayoutPolicy_UsesVisibleHeightWhenBufferIsTightlyPacked) {
  const wallpaper::Nv12Layout layout =
      wallpaper::ComputeNv12Layout(540, 960, static_cast<std::size_t>(960) * 540 * 3 / 2);

  EXPECT_EQ(layout.yPlaneOffsetBytes, static_cast<std::size_t>(0));
  EXPECT_EQ(layout.yPlaneBytes, static_cast<std::size_t>(960) * 540);
  EXPECT_EQ(layout.uvPlaneOffsetBytes, static_cast<std::size_t>(960) * 540);
  EXPECT_EQ(layout.uvPlaneBytes, static_cast<std::size_t>(960) * 270);
}

TEST_CASE(Nv12LayoutPolicy_UsesAlignedHeightWhenBufferContainsHiddenPaddingRows) {
  const std::size_t totalBytes = static_cast<std::size_t>(960) * 544 * 3 / 2;
  const wallpaper::Nv12Layout layout = wallpaper::ComputeNv12Layout(540, 960, totalBytes);

  EXPECT_EQ(layout.yPlaneOffsetBytes, static_cast<std::size_t>(0));
  EXPECT_EQ(layout.yPlaneBytes, static_cast<std::size_t>(960) * 540);
  EXPECT_EQ(layout.uvPlaneOffsetBytes, static_cast<std::size_t>(960) * 544);
  EXPECT_EQ(layout.uvPlaneBytes, totalBytes - static_cast<std::size_t>(960) * 544);
}

TEST_CASE(Nv12LayoutPolicy_ZeroInputReturnsEmptyLayout) {
  const wallpaper::Nv12Layout layout = wallpaper::ComputeNv12Layout(0, 960, 0);

  EXPECT_EQ(layout.yPlaneOffsetBytes, static_cast<std::size_t>(0));
  EXPECT_EQ(layout.yPlaneBytes, static_cast<std::size_t>(0));
  EXPECT_EQ(layout.uvPlaneOffsetBytes, static_cast<std::size_t>(0));
  EXPECT_EQ(layout.uvPlaneBytes, static_cast<std::size_t>(0));
}
