#include "wallpaper/quality_governor.h"

#include "test_support.h"

namespace {

wallpaper::RuntimeMetrics BuildMetrics(const double cpu, const double presentP95Ms,
                                       const double droppedRatio) {
  wallpaper::RuntimeMetrics metrics;
  metrics.cpuPercent = cpu;
  metrics.presentP95Ms = presentP95Ms;
  metrics.droppedFrameRatio = droppedRatio;
  return metrics;
}

}  // namespace

TEST_CASE(QualityGovernor_DegradesTo30OnHighLoad) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);

  const auto fps = governor.Update(BuildMetrics(8.2, 10.5, 0.03));
  EXPECT_EQ(fps, 30);
  EXPECT_EQ(governor.CurrentFps(), 30);
}

TEST_CASE(QualityGovernor_RecoversToTargetAfterStableSamples) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);
  EXPECT_EQ(governor.Update(BuildMetrics(8.5, 11.0, 0.04)), 30);

  EXPECT_EQ(governor.Update(BuildMetrics(4.0, 5.0, 0.0)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(4.2, 5.5, 0.0)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(4.3, 5.8, 0.0)), 60);
}

TEST_CASE(QualityGovernor_DisabledFollowsTargetFps) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(60);
  governor.SetEnabled(false);

  const auto fps = governor.Update(BuildMetrics(12.0, 18.0, 0.2));
  EXPECT_EQ(fps, 60);
  EXPECT_EQ(governor.CurrentFps(), 60);
}

TEST_CASE(QualityGovernor_Target30AlwaysRemains30) {
  wallpaper::QualityGovernor governor;
  governor.SetTargetFps(30);

  EXPECT_EQ(governor.Update(BuildMetrics(4.0, 5.0, 0.0)), 30);
  EXPECT_EQ(governor.Update(BuildMetrics(12.0, 20.0, 0.2)), 30);
}
