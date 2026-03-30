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

bool ShouldPresentFrame(const bool hasDecodedFrame, const bool hasLastPresentedFrame,
                        const bool staleFramePresentDue) noexcept {
  return hasDecodedFrame || (hasLastPresentedFrame && staleFramePresentDue);
}

bool ShouldPresentStaleFrame(const std::chrono::steady_clock::time_point now,
                             const std::chrono::steady_clock::time_point lastPresentAt,
                             const std::chrono::milliseconds minimumInterval) noexcept {
  if (lastPresentAt == std::chrono::steady_clock::time_point{}) {
    return true;
  }
  if (minimumInterval <= std::chrono::milliseconds(0)) {
    return true;
  }
  if (now < lastPresentAt) {
    return true;
  }
  return (now - lastPresentAt) >= minimumInterval;
}

bool ShouldDeferVideoDecodeStart(const bool configExistedBeforeLoad,
                                 const bool hasValidVideoPath) noexcept {
  return !configExistedBeforeLoad && hasValidVideoPath;
}

bool ShouldStartDeferredDecodeNow(const bool startupDecodeDeferred,
                                  const std::chrono::milliseconds deferredElapsed,
                                  const std::chrono::milliseconds minimumDefer) noexcept {
  if (!startupDecodeDeferred) {
    return true;
  }
  return deferredElapsed >= minimumDefer;
}

}  // namespace wallpaper
