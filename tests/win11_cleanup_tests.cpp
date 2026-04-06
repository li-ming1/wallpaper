#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "test_support.h"

namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

void ExpectFileDoesNotContain(const std::filesystem::path& path,
                              const std::vector<std::string_view>& patterns) {
  const std::string content = ReadTextFile(path);
  for (const std::string_view pattern : patterns) {
    EXPECT_TRUE(content.find(pattern) == std::string::npos);
  }
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
