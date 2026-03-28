#include "wallpaper/metrics_log_line.h"

#include <iomanip>
#include <sstream>

namespace wallpaper {
namespace {

const char* DecodeModeToCsv(const DecodeMode mode) {
  switch (mode) {
    case DecodeMode::kMediaFoundation:
      return "mf";
    case DecodeMode::kFallbackTicker:
      return "fallback";
    case DecodeMode::kUnknown:
    default:
      return "unknown";
  }
}

}  // namespace

std::string BuildMetricsCsvHeader() {
  return "unix_ms,session_id,target_fps,effective_fps,adaptive_quality,decode_mode,cpu_percent,"
         "private_bytes,present_p95_ms,dropped_frame_ratio\n";
}

std::string BuildMetricsCsvLine(const std::int64_t unixMs, const RuntimeMetrics& metrics,
                                const std::string_view sessionId, const int targetFps,
                                const int effectiveFps, const bool adaptiveQualityEnabled,
                                const DecodeMode decodeMode) {
  std::ostringstream out;
  out << unixMs << ',' << sessionId << ',' << targetFps << ',' << effectiveFps << ','
      << (adaptiveQualityEnabled ? 1 : 0) << ',' << DecodeModeToCsv(decodeMode) << ','
      << std::fixed << std::setprecision(3)
      << metrics.cpuPercent << ',' << metrics.privateWorkingSetBytes << ',' << metrics.presentP95Ms
      << ',' << metrics.droppedFrameRatio << '\n';
  return out.str();
}

}  // namespace wallpaper
