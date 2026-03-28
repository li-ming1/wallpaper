#include "wallpaper/long_run_load_policy.h"

#include <algorithm>

namespace wallpaper {
namespace {

constexpr double kMediumCpuPercent = 8.0;
constexpr double kHighCpuPercent = 10.0;
constexpr double kMediumPresentP95Ms = 9.0;
constexpr double kHighPresentP95Ms = 11.0;
constexpr double kMediumDroppedRatio = 0.02;
constexpr double kHighDroppedRatio = 0.04;
constexpr std::size_t kMediumWorkingSetBytes = 140U * 1024U * 1024U;
constexpr std::size_t kHighWorkingSetBytes = 170U * 1024U * 1024U;
constexpr std::size_t kTrimWorkingSetBytes = 180U * 1024U * 1024U;

constexpr int kEnterMediumSamples = 6;
constexpr int kEnterMediumFastSamples = 3;
constexpr int kEnterHighSamples = 8;
constexpr int kExitHighSamples = 8;
constexpr int kExitMediumSamples = 12;
constexpr int kTrimCooldownSamples = 30;

[[nodiscard]] int LevelToHotSleepBoostMs(const int level) noexcept {
  if (level >= 2) {
    return 8;
  }
  if (level == 1) {
    return 4;
  }
  return 0;
}

}  // namespace

LongRunLoadDecision UpdateLongRunLoadPolicy(const RuntimeMetrics& metrics, const bool hasActiveVideo,
                                            const bool stablePaused,
                                            LongRunLoadState* const state) noexcept {
  if (state == nullptr) {
    return {};
  }

  if (state->trimCooldownSamples > 0) {
    --state->trimCooldownSamples;
  }

  if (!hasActiveVideo || stablePaused) {
    *state = LongRunLoadState{};
    return {};
  }

  const bool highPressure =
      metrics.cpuPercent >= kHighCpuPercent || metrics.presentP95Ms >= kHighPresentP95Ms ||
      metrics.droppedFrameRatio >= kHighDroppedRatio ||
      metrics.privateWorkingSetBytes >= kHighWorkingSetBytes;
  const bool mediumPressure =
      highPressure || metrics.cpuPercent >= kMediumCpuPercent ||
      metrics.presentP95Ms >= kMediumPresentP95Ms ||
      metrics.droppedFrameRatio >= kMediumDroppedRatio ||
      metrics.privateWorkingSetBytes >= kMediumWorkingSetBytes;

  if (highPressure) {
    state->highPressureSamples = std::min(state->highPressureSamples + 1, 1024);
    state->mediumPressureSamples = std::min(state->mediumPressureSamples + 1, 1024);
    state->coolSamples = 0;
  } else if (mediumPressure) {
    state->mediumPressureSamples = std::min(state->mediumPressureSamples + 1, 1024);
    state->highPressureSamples = std::max(state->highPressureSamples - 1, 0);
    state->coolSamples = 0;
  } else {
    state->coolSamples = std::min(state->coolSamples + 1, 1024);
    state->mediumPressureSamples = std::max(state->mediumPressureSamples - 1, 0);
    state->highPressureSamples = std::max(state->highPressureSamples - 1, 0);
  }

  if (state->level == 0 &&
      (state->mediumPressureSamples >= kEnterMediumSamples ||
       state->highPressureSamples >= kEnterMediumFastSamples)) {
    state->level = 1;
    state->coolSamples = 0;
  }

  if (state->level <= 1 && state->highPressureSamples >= kEnterHighSamples) {
    state->level = 2;
    state->coolSamples = 0;
  }

  if (state->level == 2 && state->coolSamples >= kExitHighSamples) {
    state->level = 1;
    state->coolSamples = 0;
  }

  if (state->level == 1 && state->coolSamples >= kExitMediumSamples) {
    state->level = 0;
    state->coolSamples = 0;
  }

  LongRunLoadDecision decision;
  decision.decodeHotSleepBoostMs = LevelToHotSleepBoostMs(state->level);

  const bool allowTrimByPressure = state->level >= 1 || mediumPressure;
  if (allowTrimByPressure && metrics.privateWorkingSetBytes >= kTrimWorkingSetBytes &&
      state->trimCooldownSamples == 0) {
    decision.requestDecodeTrim = true;
    state->trimCooldownSamples = kTrimCooldownSamples;
  }

  return decision;
}

}  // namespace wallpaper
