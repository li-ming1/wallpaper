#include "wallpaper/loop_sleep_policy.h"
#include "wallpaper/source_frame_rate_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(LoopSleepPolicy_MainLoopPauseUsesLongInterval) {
  const int sleepMs = wallpaper::ComputeMainLoopSleepMs(true, true, 0ms);
  EXPECT_EQ(sleepMs, 110);
}

TEST_CASE(LoopSleepPolicy_MainLoopNoActiveVideoUsesIdleInterval) {
  const int sleepMs = wallpaper::ComputeMainLoopSleepMs(false, false, 0ms);
  EXPECT_EQ(sleepMs, 70);
}

TEST_CASE(LoopSleepPolicy_MainLoopActiveVideoUsesBoundedWait) {
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 0ms), 2);
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 3ms), 3);
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 33ms), 33);
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 100ms), 40);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoDecodeReadySleepsLonger) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(false, false, 4), 70);
}

TEST_CASE(LoopSleepPolicy_DecodePumpFrameReadyRunsHot) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, true, 6), 2);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoFrameUsesBackoff) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 1), 2);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 15), 16);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 16), 16);
}

TEST_CASE(LoopSleepPolicy_DecodePumpHotSleepTracksFpsCap) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(60, 60), 14);
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(30, 30), 28);
}

TEST_CASE(LoopSleepPolicy_DecodePumpHotSleepUsesSourceFpsForLowFpsVideo) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(30, 25), 34);
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(30, 24), 36);
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(60, 24), 36);
}

TEST_CASE(LoopSleepPolicy_HighResolutionTimerEnabledOnlyForActive60Fps) {
  EXPECT_TRUE(wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 0, wallpaper::DecodePath::kDxvaZeroCopy, false));
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      false, false, 60, 0, wallpaper::DecodePath::kDxvaZeroCopy, false));
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, true, 60, 0, wallpaper::DecodePath::kDxvaZeroCopy, false));
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, false, 30, 0, wallpaper::DecodePath::kDxvaZeroCopy, false));
}

TEST_CASE(LoopSleepPolicy_HighResolutionTimerDisabledUnderLongRunPressure) {
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 1, wallpaper::DecodePath::kDxvaZeroCopy, false));
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 2, wallpaper::DecodePath::kDxvaZeroCopy, false));
}

TEST_CASE(LoopSleepPolicy_HighResolutionTimerDisabledForCpuFallbackUnlessWarmupActive) {
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 0, wallpaper::DecodePath::kCpuRgb32Fallback, false));
  EXPECT_TRUE(wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 0, wallpaper::DecodePath::kCpuRgb32Fallback, true));
  EXPECT_TRUE(!wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 0, wallpaper::DecodePath::kCpuNv12Fallback, false));
  EXPECT_TRUE(wallpaper::ShouldUseHighResolutionTimer(
      true, false, 60, 0, wallpaper::DecodePath::kCpuNv12Fallback, true));
}
