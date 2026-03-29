#include "wallpaper/video_path_probe_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(VideoPathProbePolicy_UsesCacheWhenSamePathWithinTtl) {
  const auto now = std::chrono::steady_clock::now();
  EXPECT_TRUE(wallpaper::ShouldUseCachedVideoPathProbe(
      "D:/video.mp4", "D:/video.mp4", true, now, now - 200ms, 1500ms));
}

TEST_CASE(VideoPathProbePolicy_DoesNotUseCacheWhenPathChanged) {
  const auto now = std::chrono::steady_clock::now();
  EXPECT_TRUE(!wallpaper::ShouldUseCachedVideoPathProbe(
      "D:/video_new.mp4", "D:/video.mp4", true, now, now - 200ms, 1500ms));
}

TEST_CASE(VideoPathProbePolicy_DoesNotUseCacheWhenExpired) {
  const auto now = std::chrono::steady_clock::now();
  EXPECT_TRUE(!wallpaper::ShouldUseCachedVideoPathProbe(
      "D:/video.mp4", "D:/video.mp4", true, now, now - 2s, 1500ms));
}

TEST_CASE(VideoPathProbePolicy_SelectsRetryPathTtl) {
  EXPECT_EQ(wallpaper::SelectVideoPathProbeCacheTtl(true), 1500ms);
  EXPECT_EQ(wallpaper::SelectVideoPathProbeCacheTtl(false), 0ms);
}
