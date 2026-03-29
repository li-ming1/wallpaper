#include "wallpaper/config_store.h"

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
  EXPECT_EQ(cfg.fpsCap, 30);
  EXPECT_TRUE(cfg.pauseWhenNotDesktopContext);
  EXPECT_TRUE(!cfg.autoStart);
  EXPECT_TRUE(cfg.adaptiveQuality);
}

TEST_CASE(ConfigStore_RoundTripsCoreFields) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_roundtrip_config.json";
  std::filesystem::remove(path);

  wallpaper::Config expected;
  expected.videoPath = "D:/videos/demo.mp4";
  expected.fpsCap = 60;
  expected.autoStart = true;
  expected.pauseWhenNotDesktopContext = false;
  expected.adaptiveQuality = false;
  expected.codecPolicy = wallpaper::CodecPolicy::kH264PlusHevc;

  wallpaper::ConfigStore store(path);
  store.Save(expected);

  const auto actual = store.Load();
  EXPECT_EQ(actual.videoPath, expected.videoPath);
  EXPECT_EQ(actual.fpsCap, expected.fpsCap);
  EXPECT_EQ(actual.autoStart, expected.autoStart);
  EXPECT_EQ(actual.pauseWhenNotDesktopContext, expected.pauseWhenNotDesktopContext);
  EXPECT_EQ(actual.adaptiveQuality, expected.adaptiveQuality);
  EXPECT_EQ(static_cast<int>(actual.codecPolicy), static_cast<int>(expected.codecPolicy));
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
    out << "  \"codecPolicy\": \"h264\"\n";
    out << "}\n";
  }

  wallpaper::ConfigStore store(path);
  const auto cfg = store.Load();

  EXPECT_EQ(cfg.fpsCap, 60);
  const std::string rewritten = ReadFile(path);
  EXPECT_TRUE(rewritten.find("\"pauseOnFullscreen\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"pauseOnMaximized\"") == std::string::npos);
  EXPECT_TRUE(rewritten.find("\"pauseWhenNotDesktopContext\"") != std::string::npos);

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
