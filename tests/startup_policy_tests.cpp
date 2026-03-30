#include "wallpaper/startup_policy.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "test_support.h"

TEST_CASE(StartupPolicy_ShouldNotStartVideoWhenPathEmpty) {
  EXPECT_TRUE(!wallpaper::ShouldActivateVideoPipeline(""));
}

TEST_CASE(StartupPolicy_ShouldNotStartVideoWhenPathMissing) {
  const auto missingPath =
      std::filesystem::temp_directory_path() / "wallpaper_missing_video_for_policy.mp4";
  std::filesystem::remove(missingPath);

  EXPECT_TRUE(!wallpaper::ShouldActivateVideoPipeline(missingPath.string()));
}

TEST_CASE(StartupPolicy_ShouldStartVideoWhenPathExists) {
  const auto existingPath =
      std::filesystem::temp_directory_path() / "wallpaper_existing_video_for_policy.mp4";
  {
    std::ofstream out(existingPath, std::ios::binary | std::ios::trunc);
    out << "test";
  }

  EXPECT_TRUE(wallpaper::ShouldActivateVideoPipeline(existingPath.string()));
  std::filesystem::remove(existingPath);
}

TEST_CASE(StartupPolicy_ShouldNotPresentBeforeFirstDecodedFrame) {
  EXPECT_TRUE(!wallpaper::ShouldPresentFrame(false, false, true));
}

TEST_CASE(StartupPolicy_ShouldPresentAfterDecodedFrameArrives) {
  EXPECT_TRUE(wallpaper::ShouldPresentFrame(true, false, false));
}

TEST_CASE(StartupPolicy_ShouldNotPresentWhenReusingLastFrameBeforeKeepAliveDue) {
  EXPECT_TRUE(!wallpaper::ShouldPresentFrame(false, true, false));
}

TEST_CASE(StartupPolicy_ShouldPresentWhenReusingLastFrameAndKeepAliveDue) {
  EXPECT_TRUE(wallpaper::ShouldPresentFrame(false, true, true));
}

TEST_CASE(StartupPolicy_StaleFrameKeepAliveRequiresInterval) {
  using Clock = std::chrono::steady_clock;
  constexpr std::chrono::milliseconds kInterval(250);

  const Clock::time_point base = Clock::time_point{} + std::chrono::seconds(1);
  EXPECT_TRUE(!wallpaper::ShouldPresentStaleFrame(base + std::chrono::milliseconds(200), base,
                                                  kInterval));
  EXPECT_TRUE(wallpaper::ShouldPresentStaleFrame(base + std::chrono::milliseconds(250), base,
                                                 kInterval));
}

TEST_CASE(StartupPolicy_StaleFrameKeepAliveAllowsImmediatePresentWhenNoPriorPresent) {
  using Clock = std::chrono::steady_clock;
  constexpr std::chrono::milliseconds kInterval(250);

  EXPECT_TRUE(
      wallpaper::ShouldPresentStaleFrame(Clock::time_point{} + std::chrono::seconds(5),
                                         Clock::time_point{}, kInterval));
}

TEST_CASE(StartupPolicy_DeferDecodeOnFirstRunWithVideo) {
  EXPECT_TRUE(wallpaper::ShouldDeferVideoDecodeStart(false, true));
}

TEST_CASE(StartupPolicy_DoNotDeferDecodeWhenConfigExists) {
  EXPECT_TRUE(!wallpaper::ShouldDeferVideoDecodeStart(true, true));
}

TEST_CASE(StartupPolicy_DoNotDeferDecodeWithoutVideo) {
  EXPECT_TRUE(!wallpaper::ShouldDeferVideoDecodeStart(false, false));
}

TEST_CASE(StartupPolicy_DeferredDecodeWaitsBeforeStart) {
  EXPECT_TRUE(!wallpaper::ShouldStartDeferredDecodeNow(
      true, std::chrono::milliseconds(1200), std::chrono::milliseconds(2500)));
}

TEST_CASE(StartupPolicy_DeferredDecodeStartsAfterThreshold) {
  EXPECT_TRUE(wallpaper::ShouldStartDeferredDecodeNow(
      true, std::chrono::milliseconds(2500), std::chrono::milliseconds(2500)));
  EXPECT_TRUE(wallpaper::ShouldStartDeferredDecodeNow(
      true, std::chrono::milliseconds(3200), std::chrono::milliseconds(2500)));
}

TEST_CASE(StartupPolicy_NonDeferredDecodeStartsImmediately) {
  EXPECT_TRUE(wallpaper::ShouldStartDeferredDecodeNow(
      false, std::chrono::milliseconds(0), std::chrono::milliseconds(2500)));
}
