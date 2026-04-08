#include "wallpaper/config_store.h"
#include "wallpaper/async_file_writer.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "test_support.h"

namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

}  // namespace

TEST_CASE(ConfigStore_LoadsDefaultsWhenMissing) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_missing_config.json";
  std::filesystem::remove(path);

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();

  EXPECT_TRUE(cfg.videoPath.empty());
  EXPECT_TRUE(cfg.pauseWhenNotDesktopContext);
  EXPECT_TRUE(!cfg.autoStart);
  EXPECT_FALSE(cfg.debugMetrics);
  EXPECT_EQ(static_cast<int>(cfg.playbackProfile),
            static_cast<int>(wallpaper::PlaybackProfile::kBalanced));
}

TEST_CASE(ConfigStore_RoundTripsCoreFields) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_roundtrip_config.json";
  std::filesystem::remove(path);

  wallpaper::Config expected;
  expected.videoPath = "D:/videos/demo.mp4";
  expected.autoStart = true;
  expected.pauseWhenNotDesktopContext = false;
  expected.debugMetrics = true;
  expected.playbackProfile = wallpaper::PlaybackProfile::kLowCpu;

  wallpaper::ConfigStore store(path);
  store.Save(expected);

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"debugMetrics\": true") != std::string::npos);
  EXPECT_TRUE(rewritten.find("\"playbackProfile\": \"low_cpu\"") != std::string::npos);

  const auto actual = store.Load();
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);
  EXPECT_EQ(actual.debugMetrics, expected.debugMetrics);
  EXPECT_EQ(static_cast<int>(actual.playbackProfile),
            static_cast<int>(expected.playbackProfile));
}

TEST_CASE(ConfigStore_DoesNotPersistManualFpsFields) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_no_manual_fps_config.json";
  std::filesystem::remove(path);

  wallpaper::Config cfg;
  cfg.videoPath = "D:/videos/demo.mp4";
  cfg.autoStart = true;
  cfg.pauseWhenNotDesktopContext = false;
  cfg.debugMetrics = false;
  cfg.playbackProfile = wallpaper::PlaybackProfile::kBalanced;

  wallpaper::ConfigStore store(path);
  store.Save(cfg);

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"fpsCap\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"renderCapMode\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"frameLatencyWaitableMode\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"debugMetrics\": false") != std::string::npos);

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadIgnoresLegacyFieldsWithoutRewrite) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_legacy_pause_config.json";
  const std::string legacyJson =
      "{\n"
      "  \"videoPath\": \"D:/videos/demo.mp4\",\n"
      "  \"fpsCap\": 75,\n"
      "  \"autoStart\": false,\n"
      "  \"pauseOnFullscreen\": true,\n"
      "  \"pauseOnMaximized\": true,\n"
      "  \"adaptiveQuality\": true,\n"
      "  \"codecPolicy\": \"h264\",\n"
      "  \"frameLatencyWaitableMode\": true\n"
      "}\n";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << legacyJson;
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();
  EXPECT_EQ(cfg.videoPath, "D:/videos/demo.mp4");
  EXPECT_TRUE(!cfg.autoStart);
  EXPECT_TRUE(cfg.pauseWhenNotDesktopContext);
  EXPECT_FALSE(cfg.debugMetrics);
  EXPECT_EQ(static_cast<int>(cfg.playbackProfile),
            static_cast<int>(wallpaper::PlaybackProfile::kBalanced));

  const std::string rewritten = ReadFile(path);
  EXPECT_EQ(rewritten, legacyJson);

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadIgnoresUnknownFieldsWithoutRewrite) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_invalid_modes_config.json";
  const std::string unknownFieldJson =
      "{\n"
      "  \"videoPath\": \"D:/videos/demo.mp4\",\n"
      "  \"playbackProfile\": \"warp_speed\",\n"
      "  \"fpsCap\": 30,\n"
      "  \"autoStart\": false,\n"
      "  \"pauseWhenNotDesktopContext\": true,\n"
      "  \"adaptiveQuality\": true,\n"
      "  \"codecPolicy\": \"h264\",\n"
      "  \"renderCapMode\": \"warp\",\n"
      "  \"frameLatencyWaitableMode\": true\n"
      "}\n";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << unknownFieldJson;
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();
  EXPECT_EQ(cfg.videoPath, "D:/videos/demo.mp4");
  EXPECT_TRUE(!cfg.autoStart);
  EXPECT_TRUE(cfg.pauseWhenNotDesktopContext);
  EXPECT_FALSE(cfg.debugMetrics);
  EXPECT_EQ(static_cast<int>(cfg.playbackProfile),
            static_cast<int>(wallpaper::PlaybackProfile::kBalanced));

  const std::string rewritten = ReadFile(path);
  EXPECT_EQ(rewritten, unknownFieldJson);

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_ExistsReflectsConfigFileState) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_exists_config.json";
  std::filesystem::remove(path);

  wallpaper::ConfigStore store(path);
  EXPECT_TRUE(!store.Exists());

  wallpaper::Config cfg;
  cfg.videoPath = "D:/videos/demo.mp4";
  store.Save(cfg);
  EXPECT_TRUE(store.Exists());

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadExpectedFailsWhenFileMissing) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_expected_missing_config.json";
  std::filesystem::remove(path);

  wallpaper::ConfigStore store(path);
  const auto loaded = store.LoadExpected();
  EXPECT_TRUE(!loaded.has_value());
}

TEST_CASE(ConfigStore_SaveExpectedRoundTripSucceeds) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_expected_roundtrip_config.json";
  std::filesystem::remove(path);

  wallpaper::ConfigStore store(path);
  wallpaper::Config expected;
  expected.videoPath = "D:/videos/expected_demo.mp4";
  expected.autoStart = true;
  expected.pauseWhenNotDesktopContext = false;
  expected.debugMetrics = true;
  expected.playbackProfile = wallpaper::PlaybackProfile::kLowCpu;

  const auto saveResult = store.SaveExpected(expected);
  EXPECT_TRUE(saveResult.has_value());

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"debugMetrics\": true") != std::string::npos);
  EXPECT_TRUE(rewritten.find("\"playbackProfile\": \"low_cpu\"") != std::string::npos);

  const auto loadResult = store.LoadExpected();
  EXPECT_TRUE(loadResult.has_value());
  const auto actual = *loadResult;
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);
  EXPECT_EQ(actual.debugMetrics, expected.debugMetrics);
  EXPECT_EQ(static_cast<int>(actual.playbackProfile),
            static_cast<int>(expected.playbackProfile));

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_SaveExpectedFallsBackToSyncWhenAsyncWriterUnavailable) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_expected_fallback_config.json";
  std::filesystem::remove(path);

  wallpaper::AsyncFileWriter writer(8, /*startWorker=*/false);
  writer.FlushAndStop();

  wallpaper::ConfigStore store(path, &writer);
  wallpaper::Config expected;
  expected.videoPath = "D:/videos/fallback_demo.mp4";
  expected.autoStart = true;
  expected.pauseWhenNotDesktopContext = false;
  expected.debugMetrics = true;
  expected.playbackProfile = wallpaper::PlaybackProfile::kLowCpu;

  const auto saveResult = store.SaveExpected(expected);
  EXPECT_TRUE(saveResult.has_value());
  EXPECT_TRUE(std::filesystem::exists(path));

  const auto loaded = store.LoadExpected();
  EXPECT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->videoPath, expected.videoPath);
  EXPECT_EQ(loaded->autoStart, expected.autoStart);
  EXPECT_EQ(loaded->pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);
  EXPECT_EQ(loaded->debugMetrics, expected.debugMetrics);
  EXPECT_EQ(static_cast<int>(loaded->playbackProfile),
            static_cast<int>(expected.playbackProfile));

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadIgnoresNestedObjectKeys) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_nested_keys_config.json";
  const std::string nestedJson =
      "{\n"
      "  \"videoPath\": \"D:/videos/demo.mp4\",\n"
      "  \"meta\": {\n"
      "    \"autoStart\": true,\n"
      "    \"debugMetrics\": true,\n"
      "    \"pauseWhenNotDesktopContext\": false,\n"
      "    \"playbackProfile\": \"low_cpu\"\n"
      "  }\n"
      "}\n";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << nestedJson;
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();
  EXPECT_EQ(cfg.videoPath, "D:/videos/demo.mp4");
  EXPECT_FALSE(cfg.autoStart);
  EXPECT_FALSE(cfg.debugMetrics);
  EXPECT_TRUE(cfg.pauseWhenNotDesktopContext);
  EXPECT_EQ(static_cast<int>(cfg.playbackProfile),
            static_cast<int>(wallpaper::PlaybackProfile::kBalanced));

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadExpectedFailsOnMalformedJson) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_malformed_config.json";
  const std::string malformedJson =
      "{\n"
      "  \"videoPath\": \"D:/videos/demo.mp4\",\n"
      "  \"autoStart\": tru,\n"
      "  \"pauseWhenNotDesktopContext\": false\n"
      "}\n";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << malformedJson;
  }

  wallpaper::ConfigStore store(path);
  const auto loaded = store.LoadExpected();
  EXPECT_TRUE(!loaded.has_value());
  if (!loaded.has_value()) {
    EXPECT_EQ(static_cast<int>(loaded.error()),
              static_cast<int>(wallpaper::ConfigStoreError::kParseFailed));
  }

  std::filesystem::remove(path);
}
