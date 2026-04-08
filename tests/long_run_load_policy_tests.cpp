#include "wallpaper/long_run_load_policy.h"

#include <cstddef>

#include "test_support.h"

namespace {

wallpaper::RuntimeMetrics BuildMetrics(const double cpuPercent, const std::size_t workingSetBytes,
                                       const double presentP95Ms) {
  wallpaper::RuntimeMetrics metrics;
  metrics.cpuPercent = cpuPercent;
  metrics.privateBytes = workingSetBytes;
  metrics.workingSetBytes = workingSetBytes;
  metrics.presentP95Ms = presentP95Ms;
  return metrics;
}

}  // namespace

TEST_CASE(LongRunLoadPolicy_EscalatesToMediumAfterSustainedPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 4; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(6.8, 95U * 1024U * 1024U, 7.5), true, false,
        wallpaper::DecodePath::kDxvaZeroCopy, &state);
  }

  EXPECT_EQ(state.level, 1);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 8);
}

TEST_CASE(LongRunLoadPolicy_CpuFallbackAddsExtraBoostAtMediumPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 4; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(6.8, 95U * 1024U * 1024U, 7.5), true, false,
        wallpaper::DecodePath::kCpuRgb32Fallback, &state);
  }

  EXPECT_EQ(state.level, 1);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 14);
}

TEST_CASE(LongRunLoadPolicy_EscalatesToHighAfterSustainedHighPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 6; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(8.8, 115U * 1024U * 1024U, 10.0), true, false,
        wallpaper::DecodePath::kDxvaZeroCopy, &state);
  }

  EXPECT_EQ(state.level, 2);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 16);
}

TEST_CASE(LongRunLoadPolicy_CpuFallbackAddsExtraBoostAtHighPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 6; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(8.8, 115U * 1024U * 1024U, 10.0), true, false,
        wallpaper::DecodePath::kCpuRgb32Fallback, &state);
  }

  EXPECT_EQ(state.level, 2);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 28);
}

TEST_CASE(LongRunLoadPolicy_CpuNv12FallbackMatchesCpuFallbackBoostAtMediumPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 4; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(6.8, 95U * 1024U * 1024U, 7.5), true, false,
        wallpaper::DecodePath::kCpuNv12Fallback, &state);
  }

  EXPECT_EQ(state.level, 1);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 14);
}

TEST_CASE(LongRunLoadPolicy_CpuNv12FallbackMatchesCpuFallbackBoostAtHighPressure) {
  wallpaper::LongRunLoadState state;
  wallpaper::LongRunLoadDecision decision;
  for (int i = 0; i < 6; ++i) {
    decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(8.8, 115U * 1024U * 1024U, 10.0), true, false,
        wallpaper::DecodePath::kCpuNv12Fallback, &state);
  }

  EXPECT_EQ(state.level, 2);
  EXPECT_EQ(decision.decodeHotSleepBoostMs, 28);
}

TEST_CASE(LongRunLoadPolicy_CoolsDownWithHysteresis) {
  wallpaper::LongRunLoadState state;
  for (int i = 0; i < 8; ++i) {
    const auto decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(10.5, 150U * 1024U * 1024U, 9.5), true, false,
        wallpaper::DecodePath::kDxvaZeroCopy, &state);
    EXPECT_TRUE(decision.decodeHotSleepBoostMs >= 0);
  }

  for (int i = 0; i < 10; ++i) {
    const auto decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(3.0, 60U * 1024U * 1024U, 4.0), true, false,
        wallpaper::DecodePath::kDxvaZeroCopy, &state);
    EXPECT_TRUE(decision.decodeHotSleepBoostMs >= 0);
  }
  EXPECT_EQ(state.level, 1);

  for (int i = 0; i < 14; ++i) {
    const auto decision = wallpaper::UpdateLongRunLoadPolicy(
        BuildMetrics(3.0, 60U * 1024U * 1024U, 4.0), true, false,
        wallpaper::DecodePath::kDxvaZeroCopy, &state);
    EXPECT_TRUE(decision.decodeHotSleepBoostMs >= 0);
  }
  EXPECT_EQ(state.level, 0);
}

TEST_CASE(LongRunLoadPolicy_TrimHasCooldown) {
  wallpaper::LongRunLoadState state;

  const auto first = wallpaper::UpdateLongRunLoadPolicy(
      BuildMetrics(9.0, 190U * 1024U * 1024U, 8.0), true, false,
      wallpaper::DecodePath::kDxvaZeroCopy, &state);
  EXPECT_TRUE(first.requestDecodeTrim);

  const auto second = wallpaper::UpdateLongRunLoadPolicy(
      BuildMetrics(9.0, 190U * 1024U * 1024U, 8.0), true, false,
      wallpaper::DecodePath::kDxvaZeroCopy, &state);
  EXPECT_TRUE(!second.requestDecodeTrim);
}
