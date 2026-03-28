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

bool ResourceArbiter::ShouldPause() const noexcept {
  if (!sessionActive_ || !desktopVisible_) {
    return true;
  }
  if (pauseWhenNotDesktopContext_ && !desktopContextActive_) {
    return true;
  }

  return false;
}

bool ResourceArbiter::ShouldAllowHardSuspend() const noexcept {
  // 非桌面上下文（如全屏应用）仅做轻暂停，避免频繁 Stop/Open 导致恢复卡顿。
  return !sessionActive_ || !desktopVisible_;
}

}  // namespace wallpaper
