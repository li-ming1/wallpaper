#pragma once

#include <string>

namespace wallpaper {

struct Config final {
  std::string videoPath;
  bool autoStart = false;
  bool pauseWhenNotDesktopContext = true;
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
