#include "wallpaper/cpu_frame_buffer_pool.h"

#include "test_support.h"

TEST_CASE(CpuFrameBufferPool_ReusesReleasedBufferForSmallerRequest) {
  wallpaper::CpuFrameBufferPool pool(1);

  const std::uint8_t* firstData = nullptr;
  {
    const wallpaper::CpuFrameBufferLease firstLease = pool.Acquire(64);
    EXPECT_TRUE(firstLease.data != nullptr);
    EXPECT_EQ(firstLease.capacityBytes, 64U);
    EXPECT_TRUE(firstLease.holder != nullptr);
    firstData = firstLease.data;
  }

  const wallpaper::CpuFrameBufferLease secondLease = pool.Acquire(32);
  EXPECT_TRUE(secondLease.data != nullptr);
  EXPECT_TRUE(secondLease.holder != nullptr);
  EXPECT_EQ(secondLease.data, firstData);
  EXPECT_EQ(secondLease.capacityBytes, 64U);
}

TEST_CASE(CpuFrameBufferPool_GrowsBufferWhenLargerRequestArrives) {
  wallpaper::CpuFrameBufferPool pool(1);

  {
    const wallpaper::CpuFrameBufferLease firstLease = pool.Acquire(32);
    EXPECT_TRUE(firstLease.data != nullptr);
    EXPECT_EQ(firstLease.capacityBytes, 32U);
  }

  const wallpaper::CpuFrameBufferLease secondLease = pool.Acquire(96);
  EXPECT_TRUE(secondLease.data != nullptr);
  EXPECT_TRUE(secondLease.holder != nullptr);
  EXPECT_TRUE(secondLease.capacityBytes >= 96U);
}
