#pragma once

#include <chrono>

namespace wallpaper {

[[nodiscard]] bool ShouldHardSuspendDuringPause(std::chrono::milliseconds pausedDuration,
                                                std::chrono::milliseconds threshold) noexcept;
[[nodiscard]] bool ShouldTrimDecodeCacheDuringPause(std::chrono::milliseconds pausedDuration,
                                                    std::chrono::milliseconds threshold) noexcept;
[[nodiscard]] std::chrono::milliseconds SelectHardSuspendThreshold(
    bool allowAggressiveSuspend, std::chrono::milliseconds aggressiveThreshold,
    std::chrono::milliseconds conservativeThreshold) noexcept;
[[nodiscard]] bool ShouldWarmResumeDuringPause(bool rawPause, bool hardSuspended) noexcept;

}  // namespace wallpaper
