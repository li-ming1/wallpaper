#include "wallpaper/long_run_load_policy.h"

#include <algorithm>

namespace wallpaper {
namespace {

constexpr double kMediumCpuPercent = 6.5;
constexpr double kHighCpuPercent = 8.5;
constexpr double kMediumPresentP95Ms = 8.0;
constexpr double kHighPresentP95Ms = 10.0;
constexpr double kMediumDroppedRatio = 0.02;
constexpr double kHighDroppedRatio = 0.04;
constexpr std::size_t kMediumPrivateBytes = 90U * 1024U * 1024U;
constexpr std::size_t kHighPrivateBytes = 110U * 1024U * 1024U;
constexpr std::size_t kTrimPrivateBytes = 120U * 1024U * 1024U;

constexpr int kEnterMediumSamples = 4;
constexpr int kEnterMediumFastSamples = 3;
constexpr int kEnterHighSamples = 6;
constexpr int kExitHighSamples = 10;
constexpr int kExitMediumSamples = 14;
constexpr int kTrimCooldownSamples = 30;

[[nodiscard]] int LevelToHotSleepBoostMs(const int level, const DecodePath decodePath) noexcept {
  const bool cpuFallback = IsCpuFallbackDecodePath(decodePath);
  if (level >= 2) {
    return cpuFallback ? 28 : 16;
  }
  if (level == 1) {
    return cpuFallback ? 14 : 8;
  }
  return 0;
}

}  // namespace

LongRunLoadDecision UpdateLongRunLoadPolicy(const RuntimeMetrics& metrics, const bool hasActiveVideo,
                                            const bool stablePaused, const DecodePath decodePath,
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
      metrics.privateBytes >= kHighPrivateBytes;
  const bool mediumPressure =
      highPressure || metrics.cpuPercent >= kMediumCpuPercent ||
      metrics.presentP95Ms >= kMediumPresentP95Ms ||
      metrics.droppedFrameRatio >= kMediumDroppedRatio ||
      metrics.privateBytes >= kMediumPrivateBytes;

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
  decision.decodeHotSleepBoostMs = LevelToHotSleepBoostMs(state->level, decodePath);

  const bool allowTrimByPressure = state->level >= 1 || mediumPressure;
  if (allowTrimByPressure && metrics.privateBytes >= kTrimPrivateBytes &&
      state->trimCooldownSamples == 0) {
    decision.requestDecodeTrim = true;
    state->trimCooldownSamples = kTrimCooldownSamples;
  }

  return decision;
}

}  // namespace wallpaper
