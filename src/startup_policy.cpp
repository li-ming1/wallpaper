#include "wallpaper/startup_policy.h"

#include <filesystem>

namespace wallpaper {

bool ShouldActivateVideoPipeline(const std::string& videoPath) {
  if (videoPath.empty()) {
    return false;
  }

  const std::filesystem::path path(videoPath);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return false;
  }

  ec.clear();
  return std::filesystem::is_regular_file(path, ec) && !ec;
}

bool ShouldPresentFrame(const bool hasDecodedFrame, const bool hasLastPresentedFrame) noexcept {
  return hasDecodedFrame || hasLastPresentedFrame;
}

}  // namespace wallpaper
