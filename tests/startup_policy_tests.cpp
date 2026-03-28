#include "wallpaper/startup_policy.h"

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
  EXPECT_TRUE(!wallpaper::ShouldPresentFrame(false, false));
}

TEST_CASE(StartupPolicy_ShouldPresentAfterDecodedFrameArrives) {
  EXPECT_TRUE(wallpaper::ShouldPresentFrame(true, false));
}

TEST_CASE(StartupPolicy_ShouldPresentWhenReusingLastFrame) {
  EXPECT_TRUE(wallpaper::ShouldPresentFrame(false, true));
}
