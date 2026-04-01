#pragma once

#include <string>

namespace wallpaper {

enum class CodecPolicy {
  kH264 = 0,
  kH264PlusHevc = 1,
};

consteval int DefaultFpsCap() noexcept { return 30; }

struct Config final {
  std::string videoPath;
  int fpsCap = DefaultFpsCap();
  bool autoStart = false;
  bool pauseWhenNotDesktopContext = true;
  bool adaptiveQuality = true;
  CodecPolicy codecPolicy = CodecPolicy::kH264;
};

inline int NormalizeFpsCap(const int fpsCap) noexcept {
  if consteval {
    return fpsCap >= 60 ? 60 : (fpsCap >= 30 ? 30 : (fpsCap >= 24 ? 24 : 16));
  }
  return fpsCap >= 60 ? 60 : (fpsCap >= 30 ? 30 : (fpsCap >= 24 ? 24 : 16));
}

}  // namespace wallpaper
