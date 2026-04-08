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
    case DecodePath::kCpuNv12Fallback:
      return "cpu_nv12_fallback";
    case DecodePath::kFallbackTicker:
      return "fallback_ticker";
    case DecodePath::kUnknown:
    default:
      return "unknown";
  }
}

const char* DecodeInteropStageToCsv(const DecodeInteropStage stage) {
  switch (stage) {
    case DecodeInteropStage::kNotAttempted:
      return "not_attempted";
    case DecodeInteropStage::kSharedDeviceMissing:
      return "shared_device_missing";
    case DecodeInteropStage::kDeviceManagerCreateFailed:
      return "device_manager_create_failed";
    case DecodeInteropStage::kDeviceManagerResetFailed:
      return "device_manager_reset_failed";
    case DecodeInteropStage::kD3DManagerBindFailed:
      return "d3d_manager_bind_failed";
    case DecodeInteropStage::kEnabled:
      return "enabled";
    case DecodeInteropStage::kUnknown:
    default:
      return "unknown";
  }
}

const char* RuntimeThreadQosToCsv(const RuntimeThreadQos qos) {
  switch (qos) {
    case RuntimeThreadQos::kEco:
      return "eco";
    case RuntimeThreadQos::kNormal:
    default:
      return "normal";
  }
}

const char* RuntimePowerStateToCsv(const RuntimePowerState state) {
  switch (state) {
    case RuntimePowerState::kBatterySaver:
      return "battery_saver";
    case RuntimePowerState::kDisplayOff:
      return "display_off";
    case RuntimePowerState::kRemoteSession:
      return "remote_session";
    case RuntimePowerState::kSessionInactive:
      return "session_inactive";
    case RuntimePowerState::kNormal:
    default:
      return "normal";
  }
}

}  // namespace

std::string BuildMetricsCsvHeader() {
  return "unix_ms,session_id,target_fps,effective_fps,decode_mode,decode_path,"
         "decode_output_pixels,thread_qos,occluded,power_state,"
         "cpu_percent,private_bytes,working_set_bytes,present_p95_ms,"
         "long_run_level,decode_hot_sleep_ms,decode_interop_stage,decode_interop_hr\n";
}

std::string BuildMetricsCsvLine(const std::int64_t unixMs, const RuntimeMetrics& metrics,
                                const std::string_view sessionId, const int targetFps,
                                const int effectiveFps, const DecodeMode decodeMode,
                                const DecodePath decodePath,
                                const int longRunLevel, const int decodeHotSleepMs,
                                const DecodeInteropStage decodeInteropStage,
                                const std::int32_t decodeInteropHresult) {
  std::ostringstream out;
  out << unixMs << ',' << sessionId << ',' << targetFps << ',' << effectiveFps << ','
      << DecodeModeToCsv(decodeMode) << ','
      << DecodePathToCsv(decodePath) << ',' << metrics.decodeOutputPixels << ','
      << RuntimeThreadQosToCsv(metrics.threadQos) << ',' << (metrics.occluded ? 1 : 0) << ','
      << RuntimePowerStateToCsv(metrics.powerState) << ',' << std::fixed << std::setprecision(3)
      << metrics.cpuPercent << ',' << metrics.privateBytes << ',' << metrics.workingSetBytes
      << ',' << metrics.presentP95Ms << ',' << longRunLevel << ',' << decodeHotSleepMs << ','
      << DecodeInteropStageToCsv(decodeInteropStage) << ',' << decodeInteropHresult << '\n';
  return out.str();
}

}  // namespace wallpaper
