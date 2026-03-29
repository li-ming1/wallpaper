#include "wallpaper/runtime_trim_policy.h"

#include "test_support.h"

TEST_CASE(RuntimeTrimPolicy_DisablesActiveTrimForCpuFallback) {
  const bool shouldTrim = wallpaper::ShouldExecuteLongRunDecodeTrim(
      true, true, wallpaper::DecodePath::kCpuRgb32Fallback);
  EXPECT_TRUE(!shouldTrim);
}

TEST_CASE(RuntimeTrimPolicy_AllowsActiveTrimForGpuPath) {
  const bool shouldTrim = wallpaper::ShouldExecuteLongRunDecodeTrim(
      true, true, wallpaper::DecodePath::kDxvaZeroCopy);
  EXPECT_TRUE(shouldTrim);
}

TEST_CASE(RuntimeTrimPolicy_AllowsTrimWhenDecodeNotRunning) {
  const bool shouldTrim = wallpaper::ShouldExecuteLongRunDecodeTrim(
      true, false, wallpaper::DecodePath::kCpuRgb32Fallback);
  EXPECT_TRUE(shouldTrim);
}

TEST_CASE(RuntimeTrimPolicy_NeverTrimsWhenNotRequested) {
  const bool shouldTrim = wallpaper::ShouldExecuteLongRunDecodeTrim(
      false, true, wallpaper::DecodePath::kDxvaZeroCopy);
  EXPECT_TRUE(!shouldTrim);
}

TEST_CASE(RuntimeTrimPolicy_DisablesActiveTrimForCpuNv12Fallback) {
  const bool shouldTrim = wallpaper::ShouldExecuteLongRunDecodeTrim(
      true, true, wallpaper::DecodePath::kCpuNv12Fallback);
  EXPECT_TRUE(!shouldTrim);
}
