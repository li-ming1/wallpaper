#pragma once

#include <string>

namespace wallpaper {

enum class CodecPolicy {
  kH264 = 0,
  kH264PlusHevc = 1,
};

enum class FrameLatencyWaitableMode {
  kOff = 0,
  kAuto = 1,
};

struct Config final {
  std::string videoPath;
  bool autoStart = false;
  bool pauseWhenNotDesktopContext = true;
  CodecPolicy codecPolicy = CodecPolicy::kH264;
  FrameLatencyWaitableMode frameLatencyWaitableMode = FrameLatencyWaitableMode::kOff;
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
