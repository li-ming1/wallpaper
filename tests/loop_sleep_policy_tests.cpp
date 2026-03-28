#include "wallpaper/loop_sleep_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(LoopSleepPolicy_MainLoopPauseUsesLongInterval) {
  const int sleepMs = wallpaper::ComputeMainLoopSleepMs(true, true, 0ms);
  EXPECT_EQ(sleepMs, 80);
}

TEST_CASE(LoopSleepPolicy_MainLoopNoActiveVideoUsesIdleInterval) {
  const int sleepMs = wallpaper::ComputeMainLoopSleepMs(false, false, 0ms);
  EXPECT_EQ(sleepMs, 50);
}

TEST_CASE(LoopSleepPolicy_MainLoopActiveVideoUsesBoundedWait) {
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 0ms), 1);
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 3ms), 3);
  EXPECT_EQ(wallpaper::ComputeMainLoopSleepMs(false, true, 33ms), 20);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoDecodeReadySleepsLonger) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(false, false, 4), 30);
}

TEST_CASE(LoopSleepPolicy_DecodePumpFrameReadyRunsHot) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, true, 6), 0);
}

TEST_CASE(LoopSleepPolicy_DecodePumpNoFrameUsesBackoff) {
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 1), 2);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 11), 12);
  EXPECT_EQ(wallpaper::ComputeDecodePumpSleepMs(true, false, 12), 12);
}
