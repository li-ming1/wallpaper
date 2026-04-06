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
}

TEST_CASE(ConfigStore_RoundTripsCoreFields) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_roundtrip_config.json";
  std::filesystem::remove(path);

  wallpaper::Config expected;
  expected.videoPath = "D:/videos/demo.mp4";
  expected.autoStart = true;
  expected.pauseWhenNotDesktopContext = false;

  wallpaper::ConfigStore store(path);
  store.Save(expected);

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);

  const auto actual = store.Load();
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);
}

TEST_CASE(ConfigStore_DoesNotPersistManualFpsFields) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_no_manual_fps_config.json";
  std::filesystem::remove(path);

  wallpaper::Config cfg;
  cfg.videoPath = "D:/videos/demo.mp4";
  cfg.autoStart = true;
  cfg.pauseWhenNotDesktopContext = false;

  wallpaper::ConfigStore store(path);
  store.Save(cfg);

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"fpsCap\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"renderCapMode\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"frameLatencyWaitableMode\"") == std::string::npos);

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_LoadRewritesLegacyPauseKeys) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_legacy_pause_config.json";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "{\n";
    out << "  \"videoPath\": \"D:/videos/demo.mp4\",\n";
    out << "  \"fpsCap\": 75,\n";
    out << "  \"autoStart\": false,\n";
    out << "  \"pauseOnFullscreen\": true,\n";
    out << "  \"pauseOnMaximized\": true,\n";
    out << "  \"adaptiveQuality\": true,\n";
    out << "  \"codecPolicy\": \"h264\",\n";
    out << "  \"frameLatencyWaitableMode\": true\n";
    out << "}\n";
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();
  (void)cfg;

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"pauseOnFullscreen\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"pauseOnMaximized\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"pauseWhenNotDesktopContext\"") != std::string::npos);
  EXPECT_TRUE(rewritten.find("\"fpsCap\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"codecPolicy\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"frameLatencyWaitableMode\"") == std::string::npos);

  std::filesystem::remove(path);
}

TEST_CASE(ConfigStore_InvalidModesRewriteToDefaults) {
  const auto path =
      std::filesystem::temp_directory_path() / "wallpaper_invalid_modes_config.json";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "{\n";
    out << "  \"videoPath\": \"D:/videos/demo.mp4\",\n";
    out << "  \"fpsCap\": 30,\n";
    out << "  \"autoStart\": false,\n";
    out << "  \"pauseWhenNotDesktopContext\": true,\n";
    out << "  \"adaptiveQuality\": true,\n";
    out << "  \"codecPolicy\": \"h264\",\n";
    out << "  \"renderCapMode\": \"warp\",\n";
    out << "  \"frameLatencyWaitableMode\": true\n";
    out << "}\n";
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();
  (void)cfg;

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"renderCapMode\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"frameLatencyWaitableMode\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"warp\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"codecPolicy\"") == std::string::npos);

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

  const auto saveResult = store.SaveExpected(expected);
  EXPECT_TRUE(saveResult.has_value());

  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"adaptiveQuality\"") == std::string::npos);

  const auto loadResult = store.LoadExpected();
  EXPECT_TRUE(loadResult.has_value());
  const auto actual = *loadResult;
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);

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

  const auto saveResult = store.SaveExpected(expected);
  EXPECT_TRUE(saveResult.has_value());
  EXPECT_TRUE(std::filesystem::exists(path));

  const auto loaded = store.LoadExpected();
  EXPECT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->videoPath, expected.videoPath);
  EXPECT_EQ(loaded->autoStart, expected.autoStart);
  EXPECT_EQ(loaded->pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);

  std::filesystem::remove(path);
}
