#include "wallpaper/foreground_policy.h"

#include "test_support.h"

TEST_CASE(ForegroundPolicy_ShellClassesAreNotFullscreen) {
  EXPECT_TRUE(wallpaper::IsShellForegroundClass(L"Progman"));
  EXPECT_TRUE(wallpaper::IsShellForegroundClass(L"WorkerW"));
  EXPECT_TRUE(wallpaper::IsShellForegroundClass(L"SHELLDLL_DefView"));
  EXPECT_TRUE(wallpaper::IsShellForegroundClass(L"SysListView32"));
  EXPECT_TRUE(wallpaper::IsShellForegroundClass(L"WallpaperRenderHostWindow"));
}

TEST_CASE(ForegroundPolicy_NormalWindowCanBeFullscreen) {
  EXPECT_TRUE(wallpaper::ShouldTreatForegroundAsFullscreen(L"Chrome_WidgetWin_1", true, true));
}

TEST_CASE(ForegroundPolicy_NotVisibleOrNotCoveringIsNotFullscreen) {
  EXPECT_TRUE(!wallpaper::ShouldTreatForegroundAsFullscreen(L"Chrome_WidgetWin_1", false, true));
  EXPECT_TRUE(wallpaper::ShouldTreatForegroundAsFullscreen(L"Chrome_WidgetWin_1", true, false));
}

TEST_CASE(ForegroundPolicy_NearlyCoveringMonitorTreatsAsCovering) {
  EXPECT_TRUE(wallpaper::IsNearlyCoveringMonitor(2, 1, 1918, 1079, 0, 0, 1920, 1080, 4));
  EXPECT_TRUE(!wallpaper::IsNearlyCoveringMonitor(10, 8, 1910, 1072, 0, 0, 1920, 1080, 4));
}

TEST_CASE(ForegroundPolicy_CoverageRatioReflectsOverlapArea) {
  const double full = wallpaper::ComputeCoverageRatio(0, 0, 1920, 1080, 0, 0, 1920, 1080);
  EXPECT_TRUE(full > 0.999);

  const double mostly = wallpaper::ComputeCoverageRatio(0, 0, 1920, 1040, 0, 0, 1920, 1080);
  EXPECT_TRUE(mostly > 0.95);
  EXPECT_TRUE(mostly < 0.98);
}

TEST_CASE(ForegroundPolicy_LikelyFullscreenByCoverageRules) {
  EXPECT_TRUE(wallpaper::IsLikelyFullscreenWindow(true, 0.90));
  EXPECT_TRUE(!wallpaper::IsLikelyFullscreenWindow(true, 0.80));
  EXPECT_TRUE(wallpaper::IsLikelyFullscreenWindow(false, 0.99));
  EXPECT_TRUE(!wallpaper::IsLikelyFullscreenWindow(false, 0.95));
}
