#pragma once

#include <chrono>

namespace wallpaper {

[[nodiscard]] bool ShouldHardSuspendDuringPause(std::chrono::milliseconds pausedDuration,
                                                std::chrono::milliseconds threshold) noexcept;

}  // namespace wallpaper
