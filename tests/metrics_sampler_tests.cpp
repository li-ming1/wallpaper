#include "wallpaper/metrics_sampler.h"

#include "test_support.h"

TEST_CASE(MetricsSampler_RingBufferKeepsOnlyLatestSamples) {
  wallpaper::MetricsSampler sampler(3);

  wallpaper::RuntimeMetrics sample1;
  sample1.cpuPercent = 1.0;
  wallpaper::RuntimeMetrics sample2;
  sample2.cpuPercent = 2.0;
  wallpaper::RuntimeMetrics sample3;
  sample3.cpuPercent = 3.0;
  wallpaper::RuntimeMetrics sample4;
  sample4.cpuPercent = 4.0;

  sampler.PushSample(sample1);
  sampler.PushSample(sample2);
  sampler.PushSample(sample3);
  sampler.PushSample(sample4);

  const auto snapshot = sampler.Snapshot();
  EXPECT_EQ(snapshot.size(), static_cast<std::size_t>(3));
  EXPECT_EQ(snapshot[0].cpuPercent, 2.0);
  EXPECT_EQ(snapshot[1].cpuPercent, 3.0);
  EXPECT_EQ(snapshot[2].cpuPercent, 4.0);
}

TEST_CASE(MetricsSampler_LatestReturnsNewestSample) {
  wallpaper::MetricsSampler sampler(2);
  EXPECT_EQ(sampler.Latest().cpuPercent, 0.0);

  wallpaper::RuntimeMetrics sample1;
  sample1.cpuPercent = 12.5;
  wallpaper::RuntimeMetrics sample2;
  sample2.cpuPercent = 37.5;
  sampler.PushSample(sample1);
  sampler.PushSample(sample2);

  const auto latest = sampler.Latest();
  EXPECT_EQ(latest.cpuPercent, 37.5);
}
