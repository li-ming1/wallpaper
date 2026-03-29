#pragma once

#include "wallpaper/interfaces.h"
#include "wallpaper/metrics_sampler.h"

namespace wallpaper {

struct LongRunLoadState final {
  int level = 0;
  int mediumPressureSamples = 0;
  int highPressureSamples = 0;
  int coolSamples = 0;
  int trimCooldownSamples = 0;
};

struct LongRunLoadDecision final {
  int decodeHotSleepBoostMs = 0;
  bool requestDecodeTrim = false;
};

// 长时负载策略：在不重启管线的前提下，按持续压力动态提升 decode hot-sleep。
[[nodiscard]] LongRunLoadDecision UpdateLongRunLoadPolicy(
    const RuntimeMetrics& metrics, bool hasActiveVideo, bool stablePaused, DecodePath decodePath,
    LongRunLoadState* state) noexcept;

}  // namespace wallpaper
