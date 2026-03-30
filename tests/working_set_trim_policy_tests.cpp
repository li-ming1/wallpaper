#include "wallpaper/working_set_trim_policy.h"

#include <chrono>

#include "test_support.h"

TEST_CASE(WorkingSetTrimPolicy_DoesNotTrimBeforeWarmupThreshold) {
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetAfterActiveWarmup(
      false, true, false, std::chrono::milliseconds(7900), std::chrono::milliseconds(8000)));
}

TEST_CASE(WorkingSetTrimPolicy_TrimsAfterWarmupThreshold) {
  EXPECT_TRUE(wallpaper::ShouldTrimWorkingSetAfterActiveWarmup(
      false, true, false, std::chrono::milliseconds(8000), std::chrono::milliseconds(8000)));
}

TEST_CASE(WorkingSetTrimPolicy_DoesNotTrimWhenAlreadyExecuted) {
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetAfterActiveWarmup(
      true, true, false, std::chrono::milliseconds(12000), std::chrono::milliseconds(8000)));
}

TEST_CASE(WorkingSetTrimPolicy_DoesNotTrimWhenPlaybackIsPausedOrInactive) {
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetAfterActiveWarmup(
      false, false, false, std::chrono::milliseconds(12000), std::chrono::milliseconds(8000)));
  EXPECT_TRUE(!wallpaper::ShouldTrimWorkingSetAfterActiveWarmup(
      false, true, true, std::chrono::milliseconds(12000), std::chrono::milliseconds(8000)));
}
