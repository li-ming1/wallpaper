#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "wallpaper/interfaces.h"
#include "wallpaper/metrics_sampler.h"

namespace wallpaper {

[[nodiscard]] std::string BuildMetricsCsvHeader();

[[nodiscard]] std::string BuildMetricsCsvLine(std::int64_t unixMs, const RuntimeMetrics& metrics,
                                              std::string_view sessionId, int targetFps,
                                              int effectiveFps, bool adaptiveQualityEnabled,
                                              DecodeMode decodeMode, DecodePath decodePath,
                                              int longRunLevel, int decodeHotSleepMs,
                                              std::size_t decodeCopyBytesPerSec,
                                              DecodeInteropStage decodeInteropStage,
                                              std::int32_t decodeInteropHresult);

}  // namespace wallpaper
