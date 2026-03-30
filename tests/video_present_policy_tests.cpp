#include "wallpaper/video_present_policy.h"

#include "test_support.h"

TEST_CASE(VideoPresentPolicy_DoesNotSkipWhenNewVideoFrameArrives) {
  EXPECT_TRUE(!wallpaper::ShouldSkipRedundantVideoPresent(true, true));
}

TEST_CASE(VideoPresentPolicy_DoesNotSkipBeforeBackBufferIsPrimed) {
  EXPECT_TRUE(!wallpaper::ShouldSkipRedundantVideoPresent(false, false));
}

TEST_CASE(VideoPresentPolicy_SkipsWhenBackBufferAlreadyHasLatestVideoFrame) {
  EXPECT_TRUE(wallpaper::ShouldSkipRedundantVideoPresent(false, true));
}
