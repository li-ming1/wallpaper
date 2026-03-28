#include "wallpaper/metrics_log_file.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

#include "test_support.h"

namespace {

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& dir,
                                             const std::string& needle) {
  std::vector<std::filesystem::path> out;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    if (name.find(needle) != std::string::npos) {
      out.push_back(entry.path());
    }
  }
  return out;
}

}  // namespace

TEST_CASE(MetricsLogFile_AppendsWithHeaderWhenMissing) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_metrics_log_missing.csv";
  std::filesystem::remove(path);

  wallpaper::MetricsLogFile log(path, 1024, "a,b\n");
  EXPECT_TRUE(log.Append("1,2\n"));

  const std::string content = ReadAll(path);
  EXPECT_TRUE(content.find("a,b\n") == 0);
  EXPECT_TRUE(content.find("1,2\n") != std::string::npos);
}

TEST_CASE(MetricsLogFile_TruncatesToHeaderWhenExceedingLimit) {
  const auto path = std::filesystem::temp_directory_path() / "wallpaper_metrics_log_roll.csv";
  std::filesystem::remove(path);

  wallpaper::MetricsLogFile log(path, 32, "h1,h2\n");
  EXPECT_TRUE(log.Append("1111111111,1\n"));
  EXPECT_TRUE(log.Append("2222222222,2\n"));
  EXPECT_TRUE(log.Append("3333333333,3\n"));

  const std::string content = ReadAll(path);
  EXPECT_TRUE(content.find("h1,h2\n") == 0);
  EXPECT_TRUE(content.find("3333333333,3\n") != std::string::npos);
  EXPECT_TRUE(content.find("1111111111,1\n") == std::string::npos);
  EXPECT_TRUE(std::filesystem::file_size(path) <= 32);
}

TEST_CASE(MetricsLogFile_SplitsByDayAndKeepsRecentShards) {
  const auto dir = std::filesystem::temp_directory_path();
  const auto base = dir / "wallpaper_metrics_daily.csv";
  const auto marker = std::string("wallpaper_metrics_daily_");

  for (const auto& path : ListFiles(dir, marker)) {
    std::filesystem::remove(path);
  }
  std::filesystem::remove(base);

  std::string day = "20260327";
  wallpaper::MetricsLogFile log(
      base, 256, "x,y\n", 2, [&day]() { return day; });

  EXPECT_TRUE(log.Append("1,1\n"));
  day = "20260328";
  EXPECT_TRUE(log.Append("2,2\n"));
  day = "20260329";
  EXPECT_TRUE(log.Append("3,3\n"));

  const auto file27 = dir / "wallpaper_metrics_daily_20260327.csv";
  const auto file28 = dir / "wallpaper_metrics_daily_20260328.csv";
  const auto file29 = dir / "wallpaper_metrics_daily_20260329.csv";

  EXPECT_TRUE(!std::filesystem::exists(file27));
  EXPECT_TRUE(std::filesystem::exists(file28));
  EXPECT_TRUE(std::filesystem::exists(file29));

  const std::string content29 = ReadAll(file29);
  EXPECT_TRUE(content29.find("x,y\n") == 0);
  EXPECT_TRUE(content29.find("3,3\n") != std::string::npos);
}
