#include "wallpaper/resource_arbiter.h"

namespace wallpaper {

void ResourceArbiter::SetForegroundState(const ForegroundState state) noexcept {
  foregroundState_ = state;
}

void ResourceArbiter::SetPauseOnFullscreen(const bool enabled) noexcept {
  pauseOnFullscreen_ = enabled;
}

void ResourceArbiter::SetPauseOnMaximized(const bool enabled) noexcept {
  pauseOnMaximized_ = enabled;
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

  if (pauseOnFullscreen_ && foregroundState_ == ForegroundState::kFullscreen) {
    return true;
  }
  if (pauseOnMaximized_ && foregroundState_ == ForegroundState::kMaximized) {
    return true;
  }
  if (pauseWhenNotDesktopContext_ && !desktopContextActive_) {
    return true;
  }

  return false;
}

}  // namespace wallpaper
