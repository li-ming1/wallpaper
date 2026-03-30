#include "wallpaper/gpu_nv12_srv_cache_policy.h"

#include "test_support.h"

TEST_CASE(GpuNv12SrvCachePolicy_ResetsWhenSourceTextureChanges) {
  const wallpaper::GpuNv12SrvCacheState state{
      reinterpret_cast<const void*>(0x1000), 1920, 1080};

  const bool shouldReset = wallpaper::ShouldResetGpuNv12SrvCache(
      state, reinterpret_cast<const void*>(0x2000), 1920, 1080);

  EXPECT_TRUE(shouldReset);
}

TEST_CASE(GpuNv12SrvCachePolicy_ResetsWhenFrameSizeChanges) {
  const wallpaper::GpuNv12SrvCacheState state{
      reinterpret_cast<const void*>(0x1000), 1920, 1080};

  const bool shouldReset = wallpaper::ShouldResetGpuNv12SrvCache(
      state, reinterpret_cast<const void*>(0x1000), 2560, 1440);

  EXPECT_TRUE(shouldReset);
}

TEST_CASE(GpuNv12SrvCachePolicy_KeepsCacheForSameTextureAndSize) {
  const wallpaper::GpuNv12SrvCacheState state{
      reinterpret_cast<const void*>(0x1000), 1920, 1080};

  const bool shouldReset = wallpaper::ShouldResetGpuNv12SrvCache(
      state, reinterpret_cast<const void*>(0x1000), 1920, 1080);

  EXPECT_TRUE(!shouldReset);
}
