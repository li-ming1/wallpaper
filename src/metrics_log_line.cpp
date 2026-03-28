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

const char* DecodePathToCsv(const DecodePath path) {
  switch (path) {
    case DecodePath::kDxvaZeroCopy:
      return "dxva_zero_copy";
    case DecodePath::kCpuRgb32Fallback:
      return "cpu_rgb32_fallback";
    case DecodePath::kFallbackTicker:
      return "fallback_ticker";
    case DecodePath::kUnknown:
    default:
      return "unknown";
  }
}

}  // namespace

std::string BuildMetricsCsvHeader() {
  return "unix_ms,session_id,target_fps,effective_fps,adaptive_quality,decode_mode,decode_path,"
         "cpu_percent,private_bytes,working_set_bytes,present_p95_ms,dropped_frame_ratio,"
         "long_run_level,decode_hot_sleep_ms,decode_copy_bytes_per_sec\n";
}

std::string BuildMetricsCsvLine(const std::int64_t unixMs, const RuntimeMetrics& metrics,
                                const std::string_view sessionId, const int targetFps,
                                const int effectiveFps, const bool adaptiveQualityEnabled,
                                const DecodeMode decodeMode, const DecodePath decodePath,
                                const int longRunLevel, const int decodeHotSleepMs,
                                const std::size_t decodeCopyBytesPerSec) {
  std::ostringstream out;
  out << unixMs << ',' << sessionId << ',' << targetFps << ',' << effectiveFps << ','
      << (adaptiveQualityEnabled ? 1 : 0) << ',' << DecodeModeToCsv(decodeMode) << ','
      << DecodePathToCsv(decodePath) << ',' << std::fixed << std::setprecision(3)
      << metrics.cpuPercent << ',' << metrics.privateBytes << ',' << metrics.workingSetBytes << ','
      << metrics.presentP95Ms << ',' << metrics.droppedFrameRatio << ',' << longRunLevel << ','
      << decodeHotSleepMs << ',' << decodeCopyBytesPerSec << '\n';
  return out.str();
}

}  // namespace wallpaper
