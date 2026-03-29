#include "wallpaper/tray_thread_stop_policy.h"

#include "test_support.h"

TEST_CASE(TrayThreadStopPolicy_JoinWhenThreadJoinableEvenIfAlreadyStopped) {
  const wallpaper::TrayThreadStopDecision decision =
      wallpaper::ComputeTrayThreadStopDecision(false, true, true);
  EXPECT_TRUE(decision.shouldJoinWorker);
  EXPECT_TRUE(!decision.shouldPostQuit);
}

TEST_CASE(TrayThreadStopPolicy_PostQuitOnlyWhenRunningAndThreadIdReady) {
  const wallpaper::TrayThreadStopDecision decision =
      wallpaper::ComputeTrayThreadStopDecision(true, true, true);
  EXPECT_TRUE(decision.shouldJoinWorker);
  EXPECT_TRUE(decision.shouldPostQuit);
}

TEST_CASE(TrayThreadStopPolicy_NoPostQuitWithoutThreadId) {
  const wallpaper::TrayThreadStopDecision decision =
      wallpaper::ComputeTrayThreadStopDecision(true, false, true);
  EXPECT_TRUE(decision.shouldJoinWorker);
  EXPECT_TRUE(!decision.shouldPostQuit);
}
