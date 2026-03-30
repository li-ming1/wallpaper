#include "wallpaper/source_frame_rate_policy.h"

#include <algorithm>
#include <cmath>

namespace wallpaper {
namespace {

constexpr int kStableSampleThreshold = 4;
constexpr int kMaxHintSamples = 8;

bool Is24FpsDelta(const std::int64_t delta100ns) noexcept {
  return delta100ns >= 410000 && delta100ns <= 450000;
}

bool Is25FpsDelta(const std::int64_t delta100ns) noexcept {
  return delta100ns >= 380000 && delta100ns <= 409999;
}

bool Is30FpsDelta(const std::int64_t delta100ns) noexcept {
  return delta100ns >= 300000 && delta100ns <= 360000;
}

bool Is60FpsDelta(const std::int64_t delta100ns) noexcept {
  return delta100ns >= 120000 && delta100ns <= 220000;
}

void DecayHint(int* value) noexcept {
  if (value != nullptr && *value > 0) {
    --(*value);
  }
}

void BumpHint(int* value) noexcept {
  if (value == nullptr) {
    return;
  }
  *value = std::min(*value + 1, kMaxHintSamples);
}

int NormalizeHint(double fpsHint) noexcept {
  if (fpsHint <= 0.0) {
    return 0;
  }
  constexpr double kHintTolerance = 1.2;
  if (std::abs(fpsHint - 24.0) <= kHintTolerance) {
    return 24;
  }
  if (std::abs(fpsHint - 25.0) <= kHintTolerance) {
    return 25;
  }
  if (std::abs(fpsHint - 30.0) <= kHintTolerance) {
    return 30;
  }
  if (std::abs(fpsHint - 60.0) <= kHintTolerance) {
    return 60;
  }
  return 0;
}

}  // namespace

int UpdateSourceFrameRateState(const std::int64_t previousTimestamp100ns,
                               const std::int64_t currentTimestamp100ns,
                               SourceFrameRateState* const state) noexcept {
  if (state == nullptr) {
    return 60;
  }
  if (previousTimestamp100ns <= 0 || currentTimestamp100ns <= previousTimestamp100ns) {
    return state->sourceFps;
  }

  const std::int64_t delta100ns = currentTimestamp100ns - previousTimestamp100ns;
  if (Is24FpsDelta(delta100ns)) {
    BumpHint(&state->hint24);
    DecayHint(&state->hint25);
    DecayHint(&state->hint30);
    DecayHint(&state->hint60);
  } else if (Is25FpsDelta(delta100ns)) {
    BumpHint(&state->hint25);
    DecayHint(&state->hint24);
    DecayHint(&state->hint30);
    DecayHint(&state->hint60);
  } else if (Is30FpsDelta(delta100ns)) {
    BumpHint(&state->hint30);
    DecayHint(&state->hint24);
    DecayHint(&state->hint25);
    DecayHint(&state->hint60);
  } else if (Is60FpsDelta(delta100ns)) {
    BumpHint(&state->hint60);
    DecayHint(&state->hint24);
    DecayHint(&state->hint25);
    DecayHint(&state->hint30);
  } else {
    DecayHint(&state->hint24);
    DecayHint(&state->hint25);
    DecayHint(&state->hint30);
    DecayHint(&state->hint60);
  }

  if (state->hint24 >= kStableSampleThreshold) {
    state->sourceFps = 24;
  } else if (state->hint25 >= kStableSampleThreshold) {
    state->sourceFps = 25;
  } else if (state->hint30 >= kStableSampleThreshold) {
    state->sourceFps = 30;
  } else if (state->hint60 >= kStableSampleThreshold) {
    state->sourceFps = 60;
  }

  return state->sourceFps;
}

int NormalizeSourceFrameRateHint(const double fpsHint) noexcept { return NormalizeHint(fpsHint); }

int ApplySourceFrameRateHint(const int hintedSourceFps, SourceFrameRateState* const state) noexcept {
  if (state == nullptr) {
    return 60;
  }
  const int normalizedHint = NormalizeHint(static_cast<double>(hintedSourceFps));
  if (normalizedHint <= 0) {
    return state->sourceFps;
  }

  state->sourceFps = normalizedHint;
  state->hint24 = normalizedHint == 24 ? kStableSampleThreshold : 0;
  state->hint25 = normalizedHint == 25 ? kStableSampleThreshold : 0;
  state->hint30 = normalizedHint == 30 ? kStableSampleThreshold : 0;
  state->hint60 = normalizedHint == 60 ? kStableSampleThreshold : 0;
  return state->sourceFps;
}

void ResetSourceFrameRateState(SourceFrameRateState* const state) noexcept {
  if (state == nullptr) {
    return;
  }
  *state = SourceFrameRateState{};
}

}  // namespace wallpaper
