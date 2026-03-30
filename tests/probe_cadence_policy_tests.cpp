#include "wallpaper/probe_cadence_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(ProbeCadencePolicy_FirstProbeShouldRunImmediately) {
  const auto now = wallpaper::ProbeClock::time_point{};
  EXPECT_TRUE(wallpaper::ShouldRefreshRuntimeProbe(now, wallpaper::ProbeClock::time_point{}, 100ms));
}

TEST_CASE(ProbeCadencePolicy_ProbeShouldWaitUntilInterval) {
  const auto start = wallpaper::ProbeClock::time_point{} + 1ms;
  const auto now = start + 80ms;
  EXPECT_TRUE(!wallpaper::ShouldRefreshRuntimeProbe(now, start, 100ms));
}

TEST_CASE(ProbeCadencePolicy_ProbeShouldRunAtIntervalBoundary) {
  const auto start = wallpaper::ProbeClock::time_point{} + 1ms;
  const auto now = start + 100ms;
  EXPECT_TRUE(wallpaper::ShouldRefreshRuntimeProbe(now, start, 100ms));
}

TEST_CASE(ProbeCadencePolicy_ClockRollbackForcesRefresh) {
  const auto last = wallpaper::ProbeClock::time_point{} + 100ms;
  const auto now = wallpaper::ProbeClock::time_point{} + 10ms;
  EXPECT_TRUE(wallpaper::ShouldRefreshRuntimeProbe(now, last, 100ms));
}

TEST_CASE(ProbeCadencePolicy_SelectsAggressiveCadenceWhenRunning) {
  const auto intervals = wallpaper::SelectRuntimeProbeIntervals(false);
  EXPECT_EQ(intervals.session, 450ms);
  EXPECT_EQ(intervals.foreground, 180ms);
}

TEST_CASE(ProbeCadencePolicy_SelectsSparseCadenceWhenPaused) {
  const auto intervals = wallpaper::SelectRuntimeProbeIntervals(true);
  EXPECT_EQ(intervals.session, 1200ms);
  EXPECT_EQ(intervals.foreground, 180ms);
}

TEST_CASE(ProbeCadencePolicy_ForegroundProbeFailureStreakResetsOnSuccess) {
  EXPECT_EQ(wallpaper::UpdateForegroundProbeFailureStreak(true, 3), 0);
}

TEST_CASE(ProbeCadencePolicy_ForegroundProbeFailureStreakIncrementsOnFailure) {
  EXPECT_EQ(wallpaper::UpdateForegroundProbeFailureStreak(false, 0), 1);
  EXPECT_EQ(wallpaper::UpdateForegroundProbeFailureStreak(false, 2), 3);
}

TEST_CASE(ProbeCadencePolicy_UsesConservativeDesktopContextAfterRepeatedFailures) {
  EXPECT_TRUE(!wallpaper::ShouldUseConservativeDesktopContext(2, 3));
  EXPECT_TRUE(wallpaper::ShouldUseConservativeDesktopContext(3, 3));
}

TEST_CASE(ProbeCadencePolicy_ReusesForegroundProbeWhenWindowStableWithinInterval) {
  const auto now = wallpaper::ProbeClock::time_point{} + 2s;
  const auto lastDeepProbe = now - 200ms;
  EXPECT_TRUE(wallpaper::ShouldReuseForegroundProbeResult(0x1001, 0x1001, now, lastDeepProbe,
                                                          1200ms));
}

TEST_CASE(ProbeCadencePolicy_DoesNotReuseForegroundProbeWhenWindowChanges) {
  const auto now = wallpaper::ProbeClock::time_point{} + 2s;
  const auto lastDeepProbe = now - 200ms;
  EXPECT_TRUE(!wallpaper::ShouldReuseForegroundProbeResult(0x1001, 0x2002, now, lastDeepProbe,
                                                           1200ms));
}

TEST_CASE(ProbeCadencePolicy_DoesNotReuseForegroundProbeWhenIntervalElapsed) {
  const auto now = wallpaper::ProbeClock::time_point{} + 2s;
  const auto lastDeepProbe = now - 1500ms;
  EXPECT_TRUE(!wallpaper::ShouldReuseForegroundProbeResult(0x1001, 0x1001, now, lastDeepProbe,
                                                           1200ms));
}

TEST_CASE(ProbeCadencePolicy_DoesNotReuseForegroundProbeWithoutPriorDeepProbe) {
  const auto now = wallpaper::ProbeClock::time_point{} + 2s;
  EXPECT_TRUE(!wallpaper::ShouldReuseForegroundProbeResult(
      0x1001, 0x1001, now, wallpaper::ProbeClock::time_point{}, 1200ms));
}
