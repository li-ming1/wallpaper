#include "wallpaper/frame_latency_policy.h"

#include "test_support.h"

TEST_CASE(FrameLatencyPolicy_DisabledNeverSkips) {
  EXPECT_TRUE(!wallpaper::ShouldSkipPresentOnWaitTimeout(false, true, 1, 2));
}

TEST_CASE(FrameLatencyPolicy_UnarmedNeverSkips) {
  EXPECT_TRUE(!wallpaper::ShouldSkipPresentOnWaitTimeout(true, false, 1, 2));
}

TEST_CASE(FrameLatencyPolicy_SkipsBeforeForceThreshold) {
  EXPECT_TRUE(wallpaper::ShouldSkipPresentOnWaitTimeout(true, true, 1, 2));
}

TEST_CASE(FrameLatencyPolicy_ForcesPresentAtThreshold) {
  EXPECT_TRUE(!wallpaper::ShouldSkipPresentOnWaitTimeout(true, true, 2, 2));
  EXPECT_TRUE(!wallpaper::ShouldSkipPresentOnWaitTimeout(true, true, 3, 2));
}
