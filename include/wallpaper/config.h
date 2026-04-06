#pragma once

#include <string>
#include <string_view>

namespace wallpaper {

enum class PlaybackProfile {
  kBalanced = 0,
  kLowCpu = 1,
};

[[nodiscard]] constexpr std::string_view ToConfigString(const PlaybackProfile profile) noexcept {
  return profile == PlaybackProfile::kLowCpu ? std::string_view("low_cpu")
                                             : std::string_view("balanced");
}

[[nodiscard]] constexpr bool TryParsePlaybackProfile(const std::string_view text,
                                                     PlaybackProfile* const out) noexcept {
  if (out == nullptr) {
    return false;
  }
  if (text == "balanced") {
    *out = PlaybackProfile::kBalanced;
    return true;
  }
  if (text == "low_cpu") {
    *out = PlaybackProfile::kLowCpu;
    return true;
  }
  return false;
}

struct Config final {
  std::string videoPath;
  bool autoStart = false;
  bool pauseWhenNotDesktopContext = true;
  PlaybackProfile playbackProfile = PlaybackProfile::kBalanced;
};

inline int NormalizeFpsCap(const int fpsCap) noexcept {
  if consteval {
    return fpsCap >= 60 ? 60
                        : (fpsCap >= 30 ? 30
                                        : (fpsCap >= 25 ? 25 : (fpsCap >= 24 ? 24 : 16)));
  }
  return fpsCap >= 60 ? 60
                      : (fpsCap >= 30 ? 30
                                      : (fpsCap >= 25 ? 25 : (fpsCap >= 24 ? 24 : 16)));
}

}  // namespace wallpaper
