#pragma once

#include <string>

namespace wallpaper {

enum class CodecPolicy {
  kH264 = 0,
  kH264PlusHevc = 1,
};

struct Config final {
  std::string videoPath;
  int fpsCap = 30;
  bool autoStart = false;
  bool pauseOnFullscreen = true;
  bool adaptiveQuality = true;
  CodecPolicy codecPolicy = CodecPolicy::kH264;
};

inline int NormalizeFpsCap(const int fpsCap) noexcept {
  return fpsCap >= 60 ? 60 : 30;
}

}  // namespace wallpaper
