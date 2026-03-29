#include "wallpaper/resource_arbiter.h"

#include "test_support.h"

TEST_CASE(ResourceArbiter_KeepsRunningOnNormalWindow) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  EXPECT_TRUE(!arbiter.ShouldPause());
  EXPECT_EQ(arbiter.CurrentPowerState(), wallpaper::RuntimePowerState::kNormal);
}

TEST_CASE(ResourceArbiter_PausesWhenForegroundNotDesktopContextByDefault) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetDesktopContextActive(false);
  EXPECT_TRUE(arbiter.ShouldPause());
  EXPECT_TRUE(!arbiter.ShouldAllowHardSuspend());
}

TEST_CASE(ResourceArbiter_CanDisablePauseWhenNotDesktopContext) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetPauseWhenNotDesktopContext(false);
  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetDesktopContextActive(false);
  EXPECT_TRUE(!arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_ForegroundStateDoesNotForcePauseByItself) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kFullscreen);
  EXPECT_TRUE(!arbiter.ShouldPause());
  arbiter.SetForegroundState(wallpaper::ForegroundState::kMaximized);
  EXPECT_TRUE(!arbiter.ShouldPause());
}

TEST_CASE(ResourceArbiter_PausesWhenSessionInactive) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetSessionActive(false);
  EXPECT_TRUE(arbiter.ShouldPause());
  EXPECT_TRUE(arbiter.ShouldAllowHardSuspend());
  EXPECT_EQ(arbiter.CurrentPowerState(), wallpaper::RuntimePowerState::kSessionInactive);
}

TEST_CASE(ResourceArbiter_PausesWhenDesktopHidden) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetDesktopVisible(false);
  EXPECT_TRUE(arbiter.ShouldPause());
  EXPECT_TRUE(arbiter.ShouldAllowHardSuspend());
  EXPECT_EQ(arbiter.CurrentPowerState(), wallpaper::RuntimePowerState::kDisplayOff);
}

TEST_CASE(ResourceArbiter_PausesWhenBatterySaverActive) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetBatterySaverActive(true);
  EXPECT_TRUE(arbiter.ShouldPause());
  EXPECT_TRUE(arbiter.ShouldAllowHardSuspend());
  EXPECT_EQ(arbiter.CurrentPowerState(), wallpaper::RuntimePowerState::kBatterySaver);
}

TEST_CASE(ResourceArbiter_PausesWhenRemoteSessionActive) {
  wallpaper::ResourceArbiter arbiter;

  arbiter.SetForegroundState(wallpaper::ForegroundState::kWindowed);
  arbiter.SetRemoteSessionActive(true);
  EXPECT_TRUE(arbiter.ShouldPause());
  EXPECT_TRUE(arbiter.ShouldAllowHardSuspend());
  EXPECT_EQ(arbiter.CurrentPowerState(), wallpaper::RuntimePowerState::kRemoteSession);
}
