#pragma once

#include <chrono>

namespace wallpaper {

struct PauseSuspendThresholdProfile final {
  std::chrono::milliseconds trimDecodeCacheAfter{2500};
  std::chrono::milliseconds hardSuspendAggressive{20000};
  std::chrono::milliseconds hardSuspendConservative{90000};
};

[[nodiscard]] PauseSuspendThresholdProfile DefaultPauseSuspendThresholdProfile() noexcept;
[[nodiscard]] bool ShouldHardSuspendDuringPause(std::chrono::milliseconds pausedDuration,
                                                std::chrono::milliseconds threshold) noexcept;
[[nodiscard]] bool ShouldTrimDecodeCacheDuringPause(std::chrono::milliseconds pausedDuration,
                                                    std::chrono::milliseconds threshold) noexcept;
[[nodiscard]] std::chrono::milliseconds SelectHardSuspendThreshold(
    bool allowAggressiveSuspend, std::chrono::milliseconds aggressiveThreshold,
    std::chrono::milliseconds conservativeThreshold) noexcept;
[[nodiscard]] bool ShouldWarmResumeDuringPause(bool rawPause, bool hardSuspended) noexcept;
[[nodiscard]] std::chrono::milliseconds ComputeWarmResumeRetryDelay(int failureCount) noexcept;
[[nodiscard]] std::chrono::milliseconds ComputeResumePipelineRetryDelay(int failureCount) noexcept;

}  // namespace wallpaper
