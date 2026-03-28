#include "wallpaper/decode_output_policy.h"

#include "test_support.h"

TEST_CASE(DecodeOutputPolicy_AdaptiveOffKeepsDesktopHint) {
  const wallpaper::DecodeOutputHint hint =
      wallpaper::SelectDecodeOutputHint(3840, 2160, false, true);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_GpuPathKeepsDesktopHint) {
  const wallpaper::DecodeOutputHint hint =
      wallpaper::SelectDecodeOutputHint(3840, 2160, true, false);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathCapsPixelsWhenAdaptiveOn) {
  const wallpaper::DecodeOutputHint hint =
      wallpaper::SelectDecodeOutputHint(3840, 2160, true, true);
  EXPECT_EQ(hint.width, 1280U);
  EXPECT_EQ(hint.height, 720U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathKeepsSmallFrames) {
  const wallpaper::DecodeOutputHint hint =
      wallpaper::SelectDecodeOutputHint(1280, 720, true, true);
  EXPECT_EQ(hint.width, 1280U);
  EXPECT_EQ(hint.height, 720U);
}

TEST_CASE(DecodeOutputPolicy_ZeroInputReturnsZero) {
  const wallpaper::DecodeOutputHint hint =
      wallpaper::SelectDecodeOutputHint(0, 0, true, true);
  EXPECT_EQ(hint.width, 0U);
  EXPECT_EQ(hint.height, 0U);
}
