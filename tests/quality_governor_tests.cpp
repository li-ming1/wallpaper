#include "wallpaper/quality_governor.h"

#include "test_support.h"

namespace {

wallpaper::RuntimeMetrics BuildMetrics(const double cpu, const double presentP95Ms) {
  wallpaper::RuntimeMetrics metrics;
  metrics.cpuPercent = cpu;
  metrics.presentP95Ms = presentP95Ms;
  return metrics;
}

}  // namespace

TEST_CASE(QualityGovernor_DegradesTo30OnHighLoad) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);

  const auto fps = governor.Update(BuildMetrics(8.2, 10.5));
  EXPECT_EQ(fps, 30);
  EXPECT_EQ(governor.CurrentFps(), 30);
}

TEST_CASE(QualityGovernor_RecoversToTargetAfterStableSamples) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);
  EXPECT_EQ(governor.Update(BuildMetrics(8.5, 11.0)), 30);

  EXPECT_EQ(governor.Update(BuildMetrics(4.0, 5.0)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(4.2, 5.5)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(4.3, 5.8)), 60);
}

TEST_CASE(QualityGovernor_DisabledFollowsTargetFps) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);
  governor.SetEnabled(false);

  const auto fps = governor.Update(BuildMetrics(12.0, 18.0));
  EXPECT_EQ(fps, 60);
  EXPECT_EQ(governor.CurrentFps(), 60);
}

TEST_CASE(QualityGovernor_Target30AlwaysRemains30) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(30);

  EXPECT_EQ(governor.Update(BuildMetrics(4.0, 5.0)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(12.0, 20.0)), 30);
}

TEST_CASE(QualityGovernor_Target24Remains24WhenAdaptiveEnabled) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(24);

  EXPECT_EQ(governor.CurrentFps(), 24);
  EXPECT_EQ(governor.Update(BuildMetrics(4.0, 5.0)), 24);
  EXPECT_EQ(governor.Update(BuildMetrics(12.0, 20.0)), 24);
}

TEST_CASE(QualityGovernor_Target16Remains16WhenAdaptiveEnabled) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(16);

  EXPECT_EQ(governor.CurrentFps(), 16);
  EXPECT_EQ(governor.Update(BuildMetrics(3.0, 4.0)), 16);
  EXPECT_EQ(governor.Update(BuildMetrics(15.0, 22.0)), 16);
}
