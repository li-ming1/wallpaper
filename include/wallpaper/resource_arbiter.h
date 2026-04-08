#pragma once

#include "wallpaper/runtime_metrics.h"

namespace wallpaper {

class ResourceArbiter final {
 public:
  void SetPauseWhenNotDesktopContext(bool enabled) noexcept;
  void SetDesktopContextActive(bool active) noexcept;
  void SetSessionActive(bool active) noexcept;
  void SetDesktopVisible(bool visible) noexcept;
  void SetBatterySaverActive(bool active) noexcept;
  void SetRemoteSessionActive(bool active) noexcept;

  [[nodiscard]] bool ShouldPause() const noexcept;
  [[nodiscard]] bool ShouldAllowHardSuspend() const noexcept;
  [[nodiscard]] RuntimePowerState CurrentPowerState() const noexcept;

 private:
  bool pauseWhenNotDesktopContext_ = true;
  bool desktopContextActive_ = true;
  bool sessionActive_ = true;
  bool desktopVisible_ = true;
  bool batterySaverActive_ = false;
  bool remoteSessionActive_ = false;
};

}  // namespace wallpaper
