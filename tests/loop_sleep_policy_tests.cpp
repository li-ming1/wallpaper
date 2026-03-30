#include "wallpaper/loop_sleep_policy.h"
#include "wallpaper/decode_token_gate_policy.h"
#include "wallpaper/source_frame_rate_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(LoopSleepPolicy_MainLoopPauseUsesLongInterval) {
  const int sleepMs = wallpaper::ComputeMainLoopSleepMs(true, true, 0ms);
  EXPECT_EQ(sleepMs, 90);
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

TEST_CASE(LoopSleepPolicy_MainLoopMessageAwareWaitOnlyForPauseOrIdle) {
  EXPECT_TRUE(wallpaper::ShouldUseMainLoopMessageAwareWait(true, true));
  EXPECT_TRUE(wallpaper::ShouldUseMainLoopMessageAwareWait(false, false));
  EXPECT_TRUE(!wallpaper::ShouldUseMainLoopMessageAwareWait(false, true));
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoDecodeReadySleepsLonger) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(false, false, 4, false), 70);
}

TEST_CASE(LoopSleepPolicy_DecodePumpFrameReadyRunsHot) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, true, 6, false), 2);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoFrameUsesBackoff) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 1, false), 2);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 2, false), 4);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 4, false), 8);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 8, false), 12);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 22, false), 24);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 24, false), 24);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoFrameUsesLongerBackoffWithNotifier) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 22, true), 26);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 36, true), 40);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 40, true), 44);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 62, true), 64);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 64, true), 64);
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

TEST_CASE(LoopSleepPolicy_DecodePumpWakeDependsOnRenderCapChange) {
  EXPECT_TRUE(!wallpaper::ShouldWakeDecodePumpForRenderCapUpdate(28, 28, 30, 30));
  EXPECT_TRUE(wallpaper::ShouldWakeDecodePumpForRenderCapUpdate(28, 30, 30, 30));
  EXPECT_TRUE(wallpaper::ShouldWakeDecodePumpForRenderCapUpdate(28, 28, 30, 60));
}

TEST_CASE(LoopSleepPolicy_DecodePumpWakeNotificationUsesDedupe) {
  EXPECT_TRUE(wallpaper::ShouldNotifyDecodePumpWake(false));
  EXPECT_TRUE(!wallpaper::ShouldNotifyDecodePumpWake(true));
}

TEST_CASE(DecodeTokenGatePolicy_SkipsWhenNoDecodedSequence) {
  EXPECT_TRUE(!wallpaper::ShouldAttemptDecodedTokenConsume(false, 0, 0));
  EXPECT_TRUE(!wallpaper::ShouldAttemptDecodedTokenConsume(true, 11, 0));
}

TEST_CASE(DecodeTokenGatePolicy_ConsumesWhenNoLastPresentedFrame) {
  EXPECT_TRUE(wallpaper::ShouldAttemptDecodedTokenConsume(false, 0, 7));
}

TEST_CASE(DecodeTokenGatePolicy_SkipsWhenSequenceUnchanged) {
  EXPECT_TRUE(!wallpaper::ShouldAttemptDecodedTokenConsume(true, 42, 42));
}

TEST_CASE(DecodeTokenGatePolicy_ConsumesWhenSequenceAdvanced) {
  EXPECT_TRUE(wallpaper::ShouldAttemptDecodedTokenConsume(true, 42, 43));
}
