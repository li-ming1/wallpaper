#include "wallpaper/metrics_shard_retain_set.h"

#include "test_support.h"

TEST_CASE(MetricsShardRetainSet_KeepsNewestKeysOnly) {
  wallpaper::MetricsShardRetainSet retainSet(2);
  retainSet.Consider({"20260401", "a"});
  retainSet.Consider({"20260403", "c"});
  retainSet.Consider({"20260402", "b"});

  EXPECT_TRUE(!retainSet.Contains("20260401", "a"));
  EXPECT_TRUE(retainSet.Contains("20260402", "b"));
  EXPECT_TRUE(retainSet.Contains("20260403", "c"));
}

TEST_CASE(MetricsShardRetainSet_RejectsZeroKeepCount) {
  wallpaper::MetricsShardRetainSet retainSet(0);
  retainSet.Consider({"20260401", "a"});

  EXPECT_TRUE(!retainSet.Contains("20260401", "a"));
}
