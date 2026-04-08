#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "test_support.h"

namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
  EXPECT_TRUE(std::filesystem::exists(path));
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open());
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::size_t CountOccurrences(const std::string& content, const std::string_view pattern) {
  if (pattern.empty()) {
    return 0;
  }

  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = content.find(pattern, pos)) != std::string::npos) {
    ++count;
    pos += pattern.size();
  }
  return count;
}

std::string SliceByMarkers(const std::string& content, const std::string_view begin,
                           const std::string_view end) {
  const std::size_t beginPos = content.find(begin);
  if (beginPos == std::string::npos) {
    return {};
  }
  const std::size_t endPos = content.find(end, beginPos);
  if (endPos == std::string::npos || endPos <= beginPos) {
    return content.substr(beginPos);
  }
  return content.substr(beginPos, endPos - beginPos);
}

void ExpectFileDoesNotContain(const std::filesystem::path& path,
                              const std::vector<std::string_view>& patterns) {
  EXPECT_TRUE(std::filesystem::exists(path));
  if (!std::filesystem::exists(path)) {
    return;
  }
  const std::string content = ReadTextFile(path);
  for (const std::string_view pattern : patterns) {
    EXPECT_TRUE(content.find(pattern) == std::string::npos);
  }
}

std::string Trim(const std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n')) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' ||
          text[end - 1] == '\n')) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

std::set<std::string> ParseCmakeCoreSources(const std::string& content) {
  std::set<std::string> sources;
  std::istringstream input(content);
  std::string line;
  bool inBlock = false;
  while (std::getline(input, line)) {
    const std::string trimmed = Trim(line);
    if (!inBlock) {
      if (trimmed.find("set(WALLPAPER_CORE_SOURCES") != std::string::npos) {
        inBlock = true;
      }
      continue;
    }
    if (trimmed == ")") {
      break;
    }
    if (trimmed.rfind("src/", 0) == 0 && trimmed.size() >= 4 &&
        trimmed.substr(trimmed.size() - 4) == ".cpp") {
      sources.insert(trimmed);
    }
  }
  return sources;
}

std::set<std::string> ParsePsSources(const std::string& content, const std::string_view blockStart) {
  std::set<std::string> sources;
  std::istringstream input(content);
  std::string line;
  bool inBlock = false;
  while (std::getline(input, line)) {
    const std::string trimmed = Trim(line);
    if (!inBlock) {
      if (trimmed.find(blockStart) != std::string::npos) {
        inBlock = true;
      }
      continue;
    }
    if (trimmed == ")") {
      break;
    }
    const std::size_t firstQuote = trimmed.find('"');
    if (firstQuote == std::string::npos) {
      continue;
    }
    const std::size_t secondQuote = trimmed.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
      continue;
    }
    const std::string entry = trimmed.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    if (entry.rfind("src/", 0) == 0 && entry.size() >= 4 &&
        entry.substr(entry.size() - 4) == ".cpp") {
      sources.insert(entry);
    }
  }
  return sources;
}

}  // namespace

TEST_CASE(Win11Cleanup_MainlineDoesNotKeepPlatformStubs) {
  const auto repoRoot = std::filesystem::current_path();
  const auto platformStubs = repoRoot / "src" / "platform_stubs.cpp";
  EXPECT_TRUE(!std::filesystem::exists(platformStubs));

  const std::string cmakeLists = ReadTextFile(repoRoot / "CMakeLists.txt");
  EXPECT_TRUE(cmakeLists.find("src/platform_stubs.cpp") == std::string::npos);
}

TEST_CASE(Win11Cleanup_MainAndAppDoNotKeepNonWindowsFallbackGuards) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string mainCpp = ReadTextFile(repoRoot / "src" / "main.cpp");
  const std::string appCpp = ReadTextFile(repoRoot / "src" / "app.cpp");

  EXPECT_TRUE(mainCpp.find("_WIN32") == std::string::npos);
  EXPECT_TRUE(appCpp.find("#ifndef _WIN32") == std::string::npos);
}

TEST_CASE(Win11Cleanup_TargetedFilesDoNotKeepLegacyWin32Guards) {
  const auto repoRoot = std::filesystem::current_path();
  const std::vector<std::filesystem::path> targetedFiles = {
      repoRoot / "src" / "app_autostart.cpp",
      repoRoot / "src" / "app_decode_control.cpp",
      repoRoot / "src" / "app_metrics.cpp",
      repoRoot / "src" / "frame_bridge.cpp",
      repoRoot / "src" / "metrics_log_file.cpp",
      repoRoot / "src" / "video_path_matcher.cpp",
      repoRoot / "src" / "win" / "decode_pipeline_core.cpp",
      repoRoot / "src" / "win" / "decode_pipeline_internal.h",
      repoRoot / "src" / "win" / "decode_pipeline_mf.cpp",
      repoRoot / "src" / "win" / "tray_controller_win.cpp",
      repoRoot / "src" / "win" / "wallpaper_host_win.cpp",
      repoRoot / "include" / "wallpaper" / "d3d11_interop_device.h",
      repoRoot / "include" / "wallpaper" / "frame_bridge.h",
      repoRoot / "tests" / "video_path_matcher_tests.cpp",
  };

  for (const auto& path : targetedFiles) {
    ExpectFileDoesNotContain(path, {"_WIN32"});
  }
}

TEST_CASE(Win11Cleanup_TargetedFilesDoNotKeepLegacyElseStubs) {
  const auto repoRoot = std::filesystem::current_path();
  const std::vector<std::filesystem::path> targetedFiles = {
      repoRoot / "src" / "app_autostart.cpp",
      repoRoot / "src" / "app_metrics.cpp",
      repoRoot / "src" / "frame_bridge.cpp",
      repoRoot / "src" / "metrics_log_file.cpp",
      repoRoot / "src" / "video_path_matcher.cpp",
      repoRoot / "src" / "win" / "decode_pipeline_core.cpp",
      repoRoot / "src" / "win" / "decode_pipeline_mf.cpp",
      repoRoot / "src" / "win" / "tray_controller_win.cpp",
      repoRoot / "src" / "win" / "wallpaper_host_win.cpp",
      repoRoot / "include" / "wallpaper" / "frame_bridge.h",
      repoRoot / "tests" / "video_path_matcher_tests.cpp",
  };

  for (const auto& path : targetedFiles) {
    ExpectFileDoesNotContain(path, {"#else"});
  }
}

TEST_CASE(Win11Cleanup_FrameBridgeUsesTypedD3DTextures) {
  const auto repoRoot = std::filesystem::current_path();
  const auto frameBridgeHeader = repoRoot / "include" / "wallpaper" / "frame_bridge.h";
  ExpectFileDoesNotContain(frameBridgeHeader, {"void* gpuTexture", "void* gpuAuxTexture"});
}

TEST_CASE(Win11Cleanup_FrameBridgeDoesNotKeepVectorSpecialCaseForRgbaPublish) {
  const auto repoRoot = std::filesystem::current_path();
  const auto frameBridgeHeader = repoRoot / "include" / "wallpaper" / "frame_bridge.h";
  const auto frameBridgeSource = repoRoot / "src" / "frame_bridge.cpp";

  ExpectFileDoesNotContain(frameBridgeHeader,
                           {"std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels",
                            "void PublishLatestFrame("});
  ExpectFileDoesNotContain(frameBridgeSource,
                           {"std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels",
                            "void PublishLatestFrame("});
}

TEST_CASE(Win11Cleanup_MetricsLogFilePruneDoesNotEnumerateDirectoryTwice) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string metricsLogSource = ReadTextFile(repoRoot / "src" / "metrics_log_file.cpp");

  EXPECT_EQ(CountOccurrences(metricsLogSource, "std::filesystem::directory_iterator("),
            static_cast<std::size_t>(1));
}

TEST_CASE(Win11Cleanup_PathHandlingDoesNotUseFixedMaxPathBuffers) {
  const auto repoRoot = std::filesystem::current_path();
  const auto mainSource = repoRoot / "src" / "main.cpp";
  const auto autoStartSource = repoRoot / "src" / "app_autostart.cpp";
  const auto traySource = repoRoot / "src" / "win" / "tray_controller_win.cpp";

  ExpectFileDoesNotContain(mainSource, {"MAX_PATH"});
  ExpectFileDoesNotContain(autoStartSource, {"MAX_PATH"});
  ExpectFileDoesNotContain(traySource, {"MAX_PATH", "wchar_t fileBuffer["});
}

TEST_CASE(Win11Cleanup_MetricsLogFileAppendAvoidsPerLineMetadataProbe) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string metricsLogSource = ReadTextFile(repoRoot / "src" / "metrics_log_file.cpp");
  const std::string appendBlock = SliceByMarkers(metricsLogSource, "bool MetricsLogFile::Append(",
                                                  "bool MetricsLogFile::RefreshActiveShardSizeCache(");

  EXPECT_TRUE(!appendBlock.empty());
  EXPECT_TRUE(appendBlock.find("std::filesystem::file_size(") == std::string::npos);
  EXPECT_TRUE(appendBlock.find("std::filesystem::exists(") == std::string::npos);
}

TEST_CASE(Win11Cleanup_DecodePublishConvertsContiguousBufferOncePerFramePath) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string decodeMfSource = ReadTextFile(repoRoot / "src" / "win" / "decode_pipeline_mf.cpp");

  EXPECT_EQ(CountOccurrences(decodeMfSource, "ConvertToContiguousBuffer("),
            static_cast<std::size_t>(1));
}

TEST_CASE(Win11Cleanup_MetricsSamplerModuleRemovedFromProductionTree) {
  const auto repoRoot = std::filesystem::current_path();
  const auto metricsSamplerHeader = repoRoot / "include" / "wallpaper" / "metrics_sampler.h";
  const auto metricsSamplerSource = repoRoot / "src" / "metrics_sampler.cpp";
  const std::string cmakeLists = ReadTextFile(repoRoot / "CMakeLists.txt");
  const std::string runTestsScript = ReadTextFile(repoRoot / "scripts" / "run_tests.ps1");
  const std::string buildAppScript = ReadTextFile(repoRoot / "scripts" / "build_app.ps1");

  EXPECT_FALSE(std::filesystem::exists(metricsSamplerHeader));
  EXPECT_FALSE(std::filesystem::exists(metricsSamplerSource));
  EXPECT_TRUE(cmakeLists.find("metrics_sampler.cpp") == std::string::npos);
  EXPECT_TRUE(runTestsScript.find("metrics_sampler.cpp") == std::string::npos);
  EXPECT_TRUE(buildAppScript.find("metrics_sampler.cpp") == std::string::npos);
}

TEST_CASE(Win11Cleanup_WallpaperHostPresentDoesNotInlineVideoRouteExecution) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string wallpaperHostSource =
      ReadTextFile(repoRoot / "src" / "win" / "wallpaper_host_win.cpp");

  ExpectFileDoesNotContain(repoRoot / "src" / "win" / "wallpaper_host_win.cpp",
                           {"const auto consumeLatestVideoFrame = [&]()",
                            "switch (routePlan.routes[routeIndex])"});
  EXPECT_TRUE(wallpaperHostSource.find("bool TryDrawLatestVideoFrame(") != std::string::npos);
  EXPECT_TRUE(wallpaperHostSource.find("bool ExecuteVideoFrameRoutePlan(") != std::string::npos);
  EXPECT_TRUE(wallpaperHostSource.find("void CommitConsumedLatestVideoFrame(") !=
              std::string::npos);
}

TEST_CASE(Win11Cleanup_StartupPolicyDoesNotKeepStaleFrameKeepAliveDeadLogic) {
  const auto repoRoot = std::filesystem::current_path();
  const auto startupPolicyHeader = repoRoot / "include" / "wallpaper" / "startup_policy.h";
  const auto startupPolicySource = repoRoot / "src" / "startup_policy.cpp";
  const auto appSource = repoRoot / "src" / "app.cpp";

  ExpectFileDoesNotContain(startupPolicyHeader,
                           {"staleFramePresentDue", "ShouldPresentStaleFrame"});
  ExpectFileDoesNotContain(startupPolicySource,
                           {"staleFramePresentDue", "ShouldPresentStaleFrame"});
  ExpectFileDoesNotContain(appSource, {"ShouldPresentStaleFrame(", "kStaleFrameKeepAliveInterval"});
}

TEST_CASE(Win11Cleanup_AppGuardsMetricsLoggingBehindDebugMetrics) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string appSource = ReadTextFile(repoRoot / "src" / "app.cpp");
  const std::string appMetricsSource = ReadTextFile(repoRoot / "src" / "app_metrics.cpp");

  EXPECT_TRUE(appSource.find("config_.debugMetrics") != std::string::npos);
  EXPECT_TRUE(appMetricsSource.find("if (config_.debugMetrics && !metricsLogFile_.Append(") !=
              std::string::npos);
}

TEST_CASE(Win11Cleanup_AppDoesNotKeepRuntimeMetricsRingBuffer) {
  const auto repoRoot = std::filesystem::current_path();
  const auto appHeader = repoRoot / "include" / "wallpaper" / "app.h";
  const auto appMetricsSource = repoRoot / "src" / "app_metrics.cpp";

  ExpectFileDoesNotContain(appHeader, {"MetricsSampler metrics_"});
  ExpectFileDoesNotContain(appMetricsSource, {"metrics_.PushSample("});
}

TEST_CASE(Win11Cleanup_AppDoesNotKeepDroppedFrameDeadPath) {
  const auto repoRoot = std::filesystem::current_path();
  const auto appHeader = repoRoot / "include" / "wallpaper" / "app.h";
  const auto appMetricsSource = repoRoot / "src" / "app_metrics.cpp";
  const auto metricsSamplerHeader = repoRoot / "include" / "wallpaper" / "metrics_sampler.h";
  const auto qualityGovernorSource = repoRoot / "src" / "quality_governor.cpp";
  const auto longRunLoadPolicySource = repoRoot / "src" / "long_run_load_policy.cpp";

  ExpectFileDoesNotContain(appHeader, {"frameDropped", "droppedFrames_", "totalFrames_"});
  ExpectFileDoesNotContain(appMetricsSource,
                           {"frameDropped", "droppedFrames_", "totalFrames_",
                            "droppedFrameRatio"});
  EXPECT_FALSE(std::filesystem::exists(metricsSamplerHeader));
  ExpectFileDoesNotContain(qualityGovernorSource, {"droppedFrameRatio"});
  ExpectFileDoesNotContain(longRunLoadPolicySource, {"droppedFrameRatio"});
}

TEST_CASE(Win11Cleanup_AppBuildsMetricsSessionIdOnlyForDebugMetrics) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string appSource = ReadTextFile(repoRoot / "src" / "app.cpp");

  EXPECT_TRUE(appSource.find("metricsSessionId_(BuildMetricsSessionId())") == std::string::npos);
  EXPECT_TRUE(appSource.find("metricsSessionId_ = BuildMetricsSessionId();") !=
              std::string::npos);
}

TEST_CASE(Win11Cleanup_AppOnlyCreatesMetricsWriterWhenDebugMetricsEnabled) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string appSource = ReadTextFile(repoRoot / "src" / "app.cpp");

  EXPECT_TRUE(appSource.find("metricsWriter_(std::make_unique<AsyncFileWriter>(256))") ==
              std::string::npos);
  EXPECT_TRUE(appSource.find("metricsWriter_ = std::make_unique<AsyncFileWriter>(256);") !=
              std::string::npos);
  EXPECT_TRUE(appSource.find("metricsWriter_.reset();") != std::string::npos);
}

TEST_CASE(Win11Cleanup_ResourceArbiterDoesNotKeepUnusedForegroundStateApi) {
  const auto repoRoot = std::filesystem::current_path();
  const auto resourceArbiterHeader = repoRoot / "include" / "wallpaper" / "resource_arbiter.h";
  const auto resourceArbiterSource = repoRoot / "src" / "resource_arbiter.cpp";

  ExpectFileDoesNotContain(resourceArbiterHeader, {"ForegroundState", "SetForegroundState"});
  ExpectFileDoesNotContain(resourceArbiterSource, {"SetForegroundState", "foregroundState_"});
}

TEST_CASE(Win11Cleanup_CoreSourcesStayInSyncAcrossCmakeAndScripts) {
  const auto repoRoot = std::filesystem::current_path();
  const std::string cmakeLists = ReadTextFile(repoRoot / "CMakeLists.txt");
  const std::string runTestsScript = ReadTextFile(repoRoot / "scripts" / "run_tests.ps1");
  const std::string buildAppScript = ReadTextFile(repoRoot / "scripts" / "build_app.ps1");

  const std::set<std::string> cmakeCoreSources = ParseCmakeCoreSources(cmakeLists);
  const std::set<std::string> runTestsCoreSources =
      ParsePsSources(runTestsScript, "$coreSources = @(");
  const std::set<std::string> buildAppCoreSources =
      ParsePsSources(buildAppScript, "$coreSources = @(");
  const std::set<std::string> buildAppSources =
      ParsePsSources(buildAppScript, "$appSources = @(");
  std::set<std::string> buildAppAllSources = buildAppCoreSources;
  buildAppAllSources.insert(buildAppSources.begin(), buildAppSources.end());

  EXPECT_EQ(runTestsCoreSources, cmakeCoreSources);
  for (const std::string& source : cmakeCoreSources) {
    EXPECT_TRUE(buildAppAllSources.find(source) != buildAppAllSources.end());
  }
}
