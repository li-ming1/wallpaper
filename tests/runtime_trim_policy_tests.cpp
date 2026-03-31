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

TEST_CASE(RuntimeTrimPolicy_WorkingSetTrimDisabledWithoutActiveVideo) {
  EXPECT_TRUE(!wallpaper::ShouldRequestWorkingSetTrim(
      false, wallpaper::DecodePath::kCpuNv12Fallback, 80U * 1024U * 1024U, 1));
}

TEST_CASE(RuntimeTrimPolicy_WorkingSetTrimDisabledForGpuDecodePath) {
  EXPECT_TRUE(!wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kDxvaZeroCopy, 80U * 1024U * 1024U, 2));
}

TEST_CASE(RuntimeTrimPolicy_WorkingSetTrimThresholdUsesLongRunLevel) {
  EXPECT_TRUE(!wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 19U * 1024U * 1024U, 0));
  EXPECT_TRUE(wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 20U * 1024U * 1024U, 0));

  EXPECT_TRUE(!wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 17U * 1024U * 1024U, 1));
  EXPECT_TRUE(wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 18U * 1024U * 1024U, 1));

  EXPECT_TRUE(!wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 15U * 1024U * 1024U, 2));
  EXPECT_TRUE(wallpaper::ShouldRequestWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 16U * 1024U * 1024U, 2));
}
