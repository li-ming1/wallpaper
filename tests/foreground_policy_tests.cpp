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
  EXPECT_TRUE(!wallpaper::ShouldTreatForegroundAsFullscreen(L"Chrome_WidgetWin_1", true, false));
}

TEST_CASE(ForegroundPolicy_NearlyCoveringMonitorTreatsAsCovering) {
  EXPECT_TRUE(wallpaper::IsNearlyCoveringMonitor(2, 1, 1918, 1079, 0, 0, 1920, 1080, 4));
  EXPECT_TRUE(!wallpaper::IsNearlyCoveringMonitor(10, 8, 1910, 1072, 0, 0, 1920, 1080, 4));
}
