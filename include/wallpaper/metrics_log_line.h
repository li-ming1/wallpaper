#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "wallpaper/interfaces.h"
#include "wallpaper/runtime_metrics.h"

namespace wallpaper {

[[nodiscard]] std::string BuildMetricsCsvHeader();

[[nodiscard]] std::string BuildMetricsCsvLine(std::int64_t unixMs, const RuntimeMetrics& metrics,
                                              std::string_view sessionId, int targetFps,
                                              int effectiveFps, DecodeMode decodeMode,
                                              DecodePath decodePath, int longRunLevel,
                                              int decodeHotSleepMs,
                                              DecodeInteropStage decodeInteropStage,
                                              std::int32_t decodeInteropHresult);

}  // namespace wallpaper
