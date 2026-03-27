#pragma once

namespace wallpaper {

enum class ForegroundState {
  kUnknown = 0,
  kWindowed = 1,
  kFullscreen = 2,
};

class ResourceArbiter final {
 public:
  void SetForegroundState(ForegroundState state) noexcept;
  void SetPauseOnFullscreen(bool enabled) noexcept;
  void SetSessionActive(bool active) noexcept;
  void SetDesktopVisible(bool visible) noexcept;

  [[nodiscard]] bool ShouldPause() const noexcept;

 private:
  ForegroundState foregroundState_ = ForegroundState::kUnknown;
  bool pauseOnFullscreen_ = true;
  bool sessionActive_ = true;
  bool desktopVisible_ = true;
};

}  // namespace wallpaper
