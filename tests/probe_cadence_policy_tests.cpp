#include "wallpaper/probe_cadence_policy.h"
#include "wallpaper/interfaces.h"

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

TEST_CASE(ProbeCadencePolicy_SessionIntervalRelaxesInStableNormalState) {
  EXPECT_EQ(wallpaper::SelectSessionProbeIntervalForState(450ms, true, false, false), 900ms);
}

TEST_CASE(ProbeCadencePolicy_SessionIntervalKeepsBaseWhenPowerOrSessionNotNormal) {
  EXPECT_EQ(wallpaper::SelectSessionProbeIntervalForState(450ms, false, false, false), 450ms);
  EXPECT_EQ(wallpaper::SelectSessionProbeIntervalForState(450ms, true, true, false), 450ms);
  EXPECT_EQ(wallpaper::SelectSessionProbeIntervalForState(450ms, true, false, true), 450ms);
}

TEST_CASE(ProbeCadencePolicy_SessionIntervalIsCappedForPausedBase) {
  EXPECT_EQ(wallpaper::SelectSessionProbeIntervalForState(1200ms, true, false, false), 1200ms);
}

TEST_CASE(ProbeCadencePolicy_MetricsSampleIntervalIsOneSecondWhenActive) {
  EXPECT_EQ(wallpaper::SelectMetricsSampleInterval(true, false, false), 1000ms);
}

TEST_CASE(ProbeCadencePolicy_MetricsSampleIntervalIsRelaxedWhenPausedOrInactive) {
  EXPECT_EQ(wallpaper::SelectMetricsSampleInterval(false, false, false), 2000ms);
  EXPECT_EQ(wallpaper::SelectMetricsSampleInterval(true, true, false), 2000ms);
}

TEST_CASE(ProbeCadencePolicy_MetricsSampleIntervalIsRelaxedWhenOccluded) {
  EXPECT_EQ(wallpaper::SelectMetricsSampleInterval(true, false, true), 2000ms);
}

TEST_CASE(ProbeCadencePolicy_RuntimeMetricsSampleIntervalRelaxesForFullQualityCpuFallback30Fps) {
  EXPECT_EQ(wallpaper::SelectRuntimeMetricsSampleInterval(
                true, false, false, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 30)
                .count(),
            2000);
}

TEST_CASE(ProbeCadencePolicy_RuntimeMetricsSampleIntervalKeepsDefaultForOtherActivePaths) {
  EXPECT_EQ(wallpaper::SelectRuntimeMetricsSampleInterval(
                true, false, false, wallpaper::DecodePath::kDxvaZeroCopy, 1920U * 1080U, 30)
                .count(),
            1000);
  EXPECT_EQ(wallpaper::SelectRuntimeMetricsSampleInterval(
                true, false, false, wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U, 30)
                .count(),
            1000);
  EXPECT_EQ(wallpaper::SelectRuntimeMetricsSampleInterval(
                true, false, false, wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, 60)
                .count(),
            1000);
}
