#include "wallpaper/resource_arbiter.h"

#include "test_support.h"

TEST_CASE(ResourceArbiter_PausesOnFullscreen) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kFullscreen);
  EXPECT_TRUE(arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_KeepsRunningOnNormalWindow) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  EXPECT_TRUE(!arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_DoesNotPauseOnMaximizedByDefault) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kMaximized);
  EXPECT_TRUE(!arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_PausesOnMaximizedWhenEnabled) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetPauseOnMaximized(true);
  arbiter.SetForegroundState(wallpaper::ForegroundState::kMaximized);
  EXPECT_TRUE(arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_PausesWhenSessionInactive) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetSessionActive(false);
  EXPECT_TRUE(arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_PausesWhenDesktopHidden) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetDesktopVisible(false);
  EXPECT_TRUE(arbiter.ShouldPause());
}
