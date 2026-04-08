#pragma once

#include <cstddef>

namespace wallpaper {

[[nodiscard]] inline bool IsExistingInstanceErrorCode(const unsigned long errorCode) noexcept {
  // ERROR_ALREADY_EXISTS / ERROR_ACCESS_DENIED / ERROR_SHARING_VIOLATION
  return errorCode == 183UL || errorCode == 5UL || errorCode == 32UL;
}

[[nodiscard]] inline bool ShouldFallbackToLocalMutex(const unsigned long globalMutexErrorCode) noexcept {
  return !IsExistingInstanceErrorCode(globalMutexErrorCode);
}

[[nodiscard]] inline bool ShouldRejectForAdditionalRunningProcess(
    const std::size_t matchingProcessCount) noexcept {
  return matchingProcessCount > 1U;
}

[[nodiscard]] inline bool ShouldAllowSingleInstanceStartup(const bool mutexAcquired,
                                                           const bool lockFileAcquired) noexcept {
  return mutexAcquired && lockFileAcquired;
}

}  // namespace wallpaper
