#include "wallpaper/video_path_probe_policy.h"

namespace wallpaper {

bool ShouldUseCachedVideoPathProbe(
    const std::string_view requestedPath, const std::string_view cachedPath,
    const bool cacheInitialized, const std::chrono::steady_clock::time_point now,
    const std::chrono::steady_clock::time_point cachedAt,
    const std::chrono::milliseconds ttl) noexcept {
  if (!cacheInitialized) {
    return false;
  }
  if (requestedPath.empty()) {
    return false;
  }
  if (requestedPath != cachedPath) {
    return false;
  }
  if (ttl <= std::chrono::milliseconds::zero()) {
    return false;
  }
  if (now < cachedAt) {
    return false;
  }
  return (now - cachedAt) <= ttl;
}

std::chrono::milliseconds SelectVideoPathProbeCacheTtl(const bool resumeRetryPath) noexcept {
  if (resumeRetryPath) {
    return std::chrono::milliseconds(1500);
  }
  return std::chrono::milliseconds::zero();
}

}  // namespace wallpaper
