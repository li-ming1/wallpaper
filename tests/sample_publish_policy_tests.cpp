#include "wallpaper/sample_publish_policy.h"

#include "test_support.h"

TEST_CASE(SamplePublishPolicy_Nv12DefaultsProbeLock2DThenContiguous) {
  wallpaper::SamplePublishStrategyCache cache;
  const auto plan = cache.BuildPlan(
      wallpaper::SamplePublishCapabilities{false, wallpaper::SamplePublishFormat::kNv12});

  EXPECT_EQ(plan.count, static_cast<std::size_t>(2));
  EXPECT_EQ(static_cast<int>(plan.strategies[0]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kNv12Lock2D));
  EXPECT_EQ(static_cast<int>(plan.strategies[1]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kNv12Contiguous));
}

TEST_CASE(SamplePublishPolicy_CachedSuccessRunsBeforeReprobeOrder) {
  wallpaper::SamplePublishStrategyCache cache;
  cache.RememberSuccess(wallpaper::SamplePublishStrategy::kNv12Contiguous);

  const auto plan = cache.BuildPlan(
      wallpaper::SamplePublishCapabilities{false, wallpaper::SamplePublishFormat::kNv12});

  EXPECT_EQ(plan.count, static_cast<std::size_t>(2));
  EXPECT_EQ(static_cast<int>(plan.strategies[0]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kNv12Contiguous));
  EXPECT_EQ(static_cast<int>(plan.strategies[1]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kNv12Lock2D));
}

TEST_CASE(SamplePublishPolicy_ResetDropsCachedStrategy) {
  wallpaper::SamplePublishStrategyCache cache;
  cache.RememberSuccess(wallpaper::SamplePublishStrategy::kNv12Contiguous);
  cache.Reset();

  const auto plan = cache.BuildPlan(
      wallpaper::SamplePublishCapabilities{false, wallpaper::SamplePublishFormat::kNv12});

  EXPECT_EQ(plan.count, static_cast<std::size_t>(2));
  EXPECT_EQ(static_cast<int>(plan.strategies[0]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kNv12Lock2D));
}

TEST_CASE(SamplePublishPolicy_DropsIncompatibleCachedStrategyWhenFormatChanges) {
  wallpaper::SamplePublishStrategyCache cache;
  cache.RememberSuccess(wallpaper::SamplePublishStrategy::kGpuDxgi);

  const auto plan = cache.BuildPlan(
      wallpaper::SamplePublishCapabilities{false, wallpaper::SamplePublishFormat::kRgba});

  EXPECT_EQ(plan.count, static_cast<std::size_t>(1));
  EXPECT_EQ(static_cast<int>(plan.strategies[0]),
            static_cast<int>(wallpaper::SamplePublishStrategy::kRgbaContiguous));
}
