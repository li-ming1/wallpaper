#include "wallpaper/config_store.h"

#include <filesystem>

#include "test_support.h"

TEST_CASE(ConfigStore_LoadsDefaultsWhenMissing) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_missing_config.json";
  std::filesystem::remove(path);

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();

  EXPECT_TRUE(cfg.videoPath.empty());
  EXPECT_EQ(cfg.fpsCap, 30);
  EXPECT_TRUE(cfg.pauseOnFullscreen);
  EXPECT_TRUE(!cfg.autoStart);
}

TEST_CASE(ConfigStore_RoundTripsCoreFields) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_roundtrip_config.json";
  std::filesystem::remove(path);

  wallpaper::Config expected;
  expected.videoPath = "D:/videos/demo.mp4";
  expected.fpsCap = 60;
  expected.autoStart = true;
  expected.pauseOnFullscreen = true;
  expected.codecPolicy = wallpaper::CodecPolicy::kH264PlusHevc;

  wallpaper::ConfigStore store(path);
  store.Save(expected);

  const auto actual = store.Load();
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.fpsCap, expected.fpsCap);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseOnFullscreen, expected.pauseOnFullscreen);
  EXPECT_EQ(static_cast<int>(actual.codecPolicy), static_cast<int>(expected.codecPolicy));
}
