#pragma once

#include <cstdint>

namespace wallpaper {

struct SourceFrameRateState final {
  int sourceFps = 60;
  int hint24 = 0;
  int hint25 = 0;
  int hint30 = 0;
  int hint60 = 0;
};

[[nodiscard]] int UpdateSourceFrameRateState(std::int64_t previousTimestamp100ns,
                                             std::int64_t currentTimestamp100ns,
                                             SourceFrameRateState* state) noexcept;

[[nodiscard]] int NormalizeSourceFrameRateHint(double fpsHint) noexcept;

[[nodiscard]] int ApplySourceFrameRateHint(int hintedSourceFps,
                                           SourceFrameRateState* state) noexcept;

void ResetSourceFrameRateState(SourceFrameRateState* state) noexcept;

}  // namespace wallpaper
