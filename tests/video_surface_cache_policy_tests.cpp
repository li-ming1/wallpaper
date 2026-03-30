#include "wallpaper/video_surface_cache_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(VideoSurfaceCachePolicy_TrimsOnSourceChangeOnlyWhenWallpaperAttached) {
  EXPECT_TRUE(wallpaper::ShouldTrimVideoSurfaceCacheOnSourceChange(true, true));
  EXPECT_TRUE(!wallpaper::ShouldTrimVideoSurfaceCacheOnSourceChange(true, false));
  EXPECT_TRUE(!wallpaper::ShouldTrimVideoSurfaceCacheOnSourceChange(false, true));
}

TEST_CASE(VideoSurfaceCachePolicy_TrimsStaticPauseOnlyAfterThreshold) {
  EXPECT_TRUE(!wallpaper::ShouldTrimVideoSurfaceCacheDuringStaticPause(true, false, 2400ms, 2500ms));
  EXPECT_TRUE(wallpaper::ShouldTrimVideoSurfaceCacheDuringStaticPause(true, false, 2500ms, 2500ms));
  EXPECT_TRUE(!wallpaper::ShouldTrimVideoSurfaceCacheDuringStaticPause(true, true, 4000ms, 2500ms));
  EXPECT_TRUE(!wallpaper::ShouldTrimVideoSurfaceCacheDuringStaticPause(false, false, 4000ms, 2500ms));
}
