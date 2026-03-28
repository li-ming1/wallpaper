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
