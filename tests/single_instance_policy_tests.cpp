#include "wallpaper/single_instance_policy.h"

#include "test_support.h"

TEST_CASE(SingleInstancePolicy_RecognizesExistingInstanceErrorCodes) {
  EXPECT_TRUE(wallpaper::IsExistingInstanceErrorCode(183));
  EXPECT_TRUE(wallpaper::IsExistingInstanceErrorCode(5));
  EXPECT_TRUE(wallpaper::IsExistingInstanceErrorCode(32));
  EXPECT_TRUE(!wallpaper::IsExistingInstanceErrorCode(2));
}

TEST_CASE(SingleInstancePolicy_FallbackToLocalMutexOnlyOnNonExistingErrors) {
  EXPECT_TRUE(!wallpaper::ShouldFallbackToLocalMutex(183));
  EXPECT_TRUE(!wallpaper::ShouldFallbackToLocalMutex(5));
  EXPECT_TRUE(!wallpaper::ShouldFallbackToLocalMutex(32));
  EXPECT_TRUE(wallpaper::ShouldFallbackToLocalMutex(2));
}

TEST_CASE(SingleInstancePolicy_RejectsWhenMoreThanOneProcessDetected) {
  EXPECT_TRUE(!wallpaper::ShouldRejectForAdditionalRunningProcess(0));
  EXPECT_TRUE(!wallpaper::ShouldRejectForAdditionalRunningProcess(1));
  EXPECT_TRUE(wallpaper::ShouldRejectForAdditionalRunningProcess(2));
}

TEST_CASE(SingleInstancePolicy_StartRequiresAllGuards) {
  EXPECT_TRUE(wallpaper::ShouldAllowSingleInstanceStartup(true, true, false));
  EXPECT_TRUE(!wallpaper::ShouldAllowSingleInstanceStartup(false, true, false));
  EXPECT_TRUE(!wallpaper::ShouldAllowSingleInstanceStartup(true, false, false));
  EXPECT_TRUE(!wallpaper::ShouldAllowSingleInstanceStartup(true, true, true));
}
