#pragma once

#include <chrono>

namespace wallpaper {

struct PauseTransitionState final {
  bool initialized = false;
  bool stablePause = false;
  bool lastRawPause = false;
  std::chrono::steady_clock::time_point lastRawChangeAt{};
};

// 将原始暂停信号变为稳定状态，减少前台窗口抖动导致的频繁切换。
[[nodiscard]] bool UpdatePauseTransition(bool rawPause,
                                         std::chrono::steady_clock::time_point now,
                                         std::chrono::milliseconds enterDelay,
                                         std::chrono::milliseconds exitDelay,
                                         PauseTransitionState* state) noexcept;

}  // namespace wallpaper
