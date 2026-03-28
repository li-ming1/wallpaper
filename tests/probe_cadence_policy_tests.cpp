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
