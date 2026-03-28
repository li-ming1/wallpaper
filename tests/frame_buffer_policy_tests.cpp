#include "wallpaper/frame_buffer_policy.h"

#include "test_support.h"

TEST_CASE(FrameBufferPolicy_GrowsWhenRequiredExceedsCapacity) {
  EXPECT_EQ(wallpaper::DecideFrameBufferCapacity(1024U, 2048U), 2048U);
}

TEST_CASE(FrameBufferPolicy_StaysWhenCapacityWithinReuseRange) {
  EXPECT_EQ(wallpaper::DecideFrameBufferCapacity(4096U, 3072U), 4096U);
}

TEST_CASE(FrameBufferPolicy_ShrinksWhenCapacityTooLarge) {
  EXPECT_EQ(wallpaper::DecideFrameBufferCapacity(8192U, 2048U), 2048U);
}

TEST_CASE(FrameBufferPolicy_ZeroRequiredReturnsZero) {
  EXPECT_EQ(wallpaper::DecideFrameBufferCapacity(2048U, 0U), 0U);
}
