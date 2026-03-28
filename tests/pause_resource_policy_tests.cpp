#include "wallpaper/pause_resource_policy.h"

#include "test_support.h"

TEST_CASE(PauseResourcePolicy_ReleasesOnlyWhenPauseAndActive) {
  EXPECT_TRUE(wallpaper::ShouldReleaseResourcesOnPause(true, true));
  EXPECT_TRUE(!wallpaper::ShouldReleaseResourcesOnPause(true, false));
  EXPECT_TRUE(!wallpaper::ShouldReleaseResourcesOnPause(false, true));
}

TEST_CASE(PauseResourcePolicy_RestoresOnlyWhenResumeAfterSuspend) {
  EXPECT_TRUE(wallpaper::ShouldRestoreResourcesOnResume(false, true));
  EXPECT_TRUE(!wallpaper::ShouldRestoreResourcesOnResume(true, true));
  EXPECT_TRUE(!wallpaper::ShouldRestoreResourcesOnResume(false, false));
}

TEST_CASE(PauseResourcePolicy_KeepsWallpaperLayerForStaticPauseFrame) {
  EXPECT_TRUE(wallpaper::ShouldKeepWallpaperLayerDuringPause(true, true));
  EXPECT_TRUE(wallpaper::ShouldKeepWallpaperLayerDuringPause(true, false));
  EXPECT_TRUE(!wallpaper::ShouldKeepWallpaperLayerDuringPause(false, true));
}
