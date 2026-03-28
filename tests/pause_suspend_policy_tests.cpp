#include "wallpaper/pause_suspend_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(PauseSuspendPolicy_DoesNotHardSuspendImmediately) {
  EXPECT_TRUE(!wallpaper::ShouldHardSuspendDuringPause(200ms, 1500ms));
}

TEST_CASE(PauseSuspendPolicy_HardSuspendsAfterThreshold) {
  EXPECT_TRUE(wallpaper::ShouldHardSuspendDuringPause(1500ms, 1500ms));
  EXPECT_TRUE(wallpaper::ShouldHardSuspendDuringPause(2200ms, 1500ms));
}

TEST_CASE(PauseSuspendPolicy_DoesNotTrimDecodeCacheImmediately) {
  EXPECT_TRUE(!wallpaper::ShouldTrimDecodeCacheDuringPause(600ms, 1500ms));
}

TEST_CASE(PauseSuspendPolicy_TrimsDecodeCacheAfterThreshold) {
  EXPECT_TRUE(wallpaper::ShouldTrimDecodeCacheDuringPause(1500ms, 1500ms));
}

TEST_CASE(PauseSuspendPolicy_SelectsAggressiveThreshold) {
  EXPECT_EQ(wallpaper::SelectHardSuspendThreshold(true, 8s, 12s), 8s);
}

TEST_CASE(PauseSuspendPolicy_SelectsConservativeThreshold) {
  EXPECT_EQ(wallpaper::SelectHardSuspendThreshold(false, 8s, 12s), 12s);
}

TEST_CASE(PauseSuspendPolicy_WarmResumeOnlyWhenLeavingPauseAfterHardSuspend) {
  EXPECT_TRUE(wallpaper::ShouldWarmResumeDuringPause(false, true));
  EXPECT_TRUE(!wallpaper::ShouldWarmResumeDuringPause(true, true));
  EXPECT_TRUE(!wallpaper::ShouldWarmResumeDuringPause(false, false));
}
