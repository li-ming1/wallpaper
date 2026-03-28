#include "wallpaper/loop_sleep_policy.h"

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
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 33ms), 24);
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
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(60), 6);
  EXPECT_EQ(wallpaper::ComputeDecodePumpHotSleepMs(30), 12);
}
