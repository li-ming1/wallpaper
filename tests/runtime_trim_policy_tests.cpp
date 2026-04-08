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

TEST_CASE(RuntimeTrimPolicy_UsesAggressiveMemoryPriorityOnlyForCompactCpuFallbackFrames) {
  EXPECT_TRUE(wallpaper::ShouldUseAggressiveMemoryPriority(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U));
  EXPECT_TRUE(wallpaper::ShouldUseAggressiveMemoryPriority(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U));
  EXPECT_TRUE(!wallpaper::ShouldUseAggressiveMemoryPriority(
      true, wallpaper::DecodePath::kDxvaZeroCopy, 960U * 540U));
  EXPECT_TRUE(!wallpaper::ShouldUseAggressiveMemoryPriority(
      false, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U));
}

TEST_CASE(RuntimeTrimPolicy_ImmediatePostPresentTrimOnlyForCompactCpuFallbackFrames) {
  EXPECT_TRUE(wallpaper::ShouldTrimWorkingSetImmediatelyAfterPresent(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U));
  EXPECT_TRUE(wallpaper::ShouldTrimWorkingSetImmediatelyAfterPresent(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U));
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetImmediatelyAfterPresent(
      true, wallpaper::DecodePath::kDxvaZeroCopy, 960U * 540U));
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetImmediatelyAfterPresent(
      false, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U));
}

TEST_CASE(RuntimeTrimPolicy_PostPresentTrimIntervalOnlyEnabledForCompactCpuFallbackFrames) {
  EXPECT_EQ(wallpaper::SelectPostPresentWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U)
                .count(),
            150);
  EXPECT_EQ(wallpaper::SelectPostPresentWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U)
                .count(),
            0);
  EXPECT_EQ(wallpaper::SelectPostPresentWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kDxvaZeroCopy, 960U * 540U)
                .count(),
            0);
  EXPECT_EQ(wallpaper::SelectPostPresentWorkingSetTrimInterval(
                false, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U)
                .count(),
            0);
}

TEST_CASE(RuntimeTrimPolicy_RuntimeWorkingSetTrimIntervalRelaxesForFullQualityCpuFallback) {
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 0)
                .count(),
            6000);
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 1)
                .count(),
            4000);
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 2)
                .count(),
            2500);
}

TEST_CASE(RuntimeTrimPolicy_RuntimeWorkingSetTrimIntervalKeepsFastFallbackForOtherCases) {
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U, 0)
                .count(),
            2000);
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                true, wallpaper::DecodePath::kDxvaZeroCopy, 1920U * 1080U, 0)
                .count(),
            0);
  EXPECT_EQ(wallpaper::SelectRuntimeWorkingSetTrimInterval(
                false, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 0)
                .count(),
            0);
}

TEST_CASE(RuntimeTrimPolicy_StartupWorkingSetTrimOnlyForFullQualitySharedGpuNv12Fallback) {
  EXPECT_TRUE(wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, true, false));
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U, true, false));
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuRgb32Fallback, 1920U * 1080U, true, false));
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kDxvaZeroCopy, 1920U * 1080U, true, false));
}

TEST_CASE(RuntimeTrimPolicy_StartupWorkingSetTrimSkipsWhenInactiveOrAlreadyDone) {
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      false, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, true, false));
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, false, false));
  EXPECT_TRUE(!wallpaper::ShouldExecuteStartupWorkingSetTrim(
      true, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, true, true));
}
