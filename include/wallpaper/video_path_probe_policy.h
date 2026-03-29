#pragma once

#include <chrono>
#include <string_view>

namespace wallpaper {

[[nodiscard]] bool ShouldUseCachedVideoPathProbe(
    std::string_view requestedPath, std::string_view cachedPath, bool cacheInitialized,
    std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point cachedAt,
    std::chrono::milliseconds ttl) noexcept;

[[nodiscard]] std::chrono::milliseconds SelectVideoPathProbeCacheTtl(bool resumeRetryPath) noexcept;

}  // namespace wallpaper
