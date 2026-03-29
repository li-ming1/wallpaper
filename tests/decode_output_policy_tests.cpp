#include "wallpaper/decode_output_policy.h"

#include "test_support.h"

TEST_CASE(DecodeOutputPolicy_AdaptiveOffKeepsDesktopHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = false;
  options.cpuFallbackPath = true;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_GpuPathKeepsDesktopHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = false;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathCapsPixelsAt720pInNormalPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 1280U);
  EXPECT_EQ(hint.height, 720U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathKeeps720pAtMediumPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 1;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 1280U);
  EXPECT_EQ(hint.height, 720U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathDropsTo540pAtHighPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 2;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 960U);
  EXPECT_EQ(hint.height, 540U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathKeepsSmallFrames) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1280;
  options.desktopHeight = 720;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 2;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 1280U);
  EXPECT_EQ(hint.height, 720U);
}

TEST_CASE(DecodeOutputPolicy_ZeroInputReturnsZero) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 0;
  options.desktopHeight = 0;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 0U);
  EXPECT_EQ(hint.height, 0U);
}
