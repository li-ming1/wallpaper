#pragma once

#include <chrono>

#include "wallpaper/config.h"

namespace wallpaper {

class RenderScheduler final {
 public:
  using Clock = std::chrono::steady_clock;

  explicit RenderScheduler(int fpsCap);

  void SetFpsCap(int fpsCap);
  [[nodiscard]] int GetFpsCap() const noexcept;
  [[nodiscard]] bool ShouldRender(Clock::time_point now);
  void Reset() noexcept;

 private:
  int fpsCap_;
  Clock::duration frameInterval_;
  Clock::time_point lastFrameAt_{};
  bool hasLastFrame_ = false;
};

}  // namespace wallpaper
