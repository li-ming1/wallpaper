#include "wallpaper/resource_arbiter.h"

namespace wallpaper {

void ResourceArbiter::SetForegroundState(const ForegroundState state) noexcept {
  foregroundState_ = state;
}

void ResourceArbiter::SetPauseWhenNotDesktopContext(const bool enabled) noexcept {
  pauseWhenNotDesktopContext_ = enabled;
}

void ResourceArbiter::SetDesktopContextActive(const bool active) noexcept {
  desktopContextActive_ = active;
}

void ResourceArbiter::SetSessionActive(const bool active) noexcept {
  sessionActive_ = active;
}

void ResourceArbiter::SetDesktopVisible(const bool visible) noexcept {
  desktopVisible_ = visible;
}

void ResourceArbiter::SetBatterySaverActive(const bool active) noexcept {
  batterySaverActive_ = active;
}

void ResourceArbiter::SetRemoteSessionActive(const bool active) noexcept {
  remoteSessionActive_ = active;
}

bool ResourceArbiter::ShouldPause() const noexcept {
  if (CurrentPowerState() != RuntimePowerState::kNormal) {
    return true;
  }
  if (pauseWhenNotDesktopContext_ && !desktopContextActive_) {
    return true;
  }

  return false;
}

bool ResourceArbiter::ShouldAllowHardSuspend() const noexcept {
  // 非桌面上下文（如全屏应用）仅做轻暂停，避免频繁 Stop/Open 导致恢复卡顿。
  return CurrentPowerState() != RuntimePowerState::kNormal;
}

RuntimePowerState ResourceArbiter::CurrentPowerState() const noexcept {
  if (!sessionActive_) {
    return RuntimePowerState::kSessionInactive;
  }
  if (!desktopVisible_) {
    return RuntimePowerState::kDisplayOff;
  }
  if (remoteSessionActive_) {
    return RuntimePowerState::kRemoteSession;
  }
  if (batterySaverActive_) {
    return RuntimePowerState::kBatterySaver;
  }
  return RuntimePowerState::kNormal;
}

}  // namespace wallpaper
