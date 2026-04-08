#include "wallpaper/metrics_log_line.h"

#include <array>
#include <charconv>
#include <cmath>
#include <type_traits>

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

template <typename T>
void AppendInteger(std::string* const out, const T value) {
  if (out == nullptr) {
    return;
  }
  std::array<char, 32> buffer{};
  char* writeEnd = buffer.data();
  if constexpr (std::is_signed_v<T> || std::is_unsigned_v<T>) {
    const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (ec != std::errc{}) {
      out->push_back('0');
      return;
    }
    writeEnd = ptr;
  } else {
    out->push_back('0');
    return;
  }
  out->append(buffer.data(), static_cast<std::size_t>(writeEnd - buffer.data()));
}

void AppendFixed3(std::string* const out, const double value) {
  if (out == nullptr) {
    return;
  }
  if (!std::isfinite(value)) {
    out->append("0.000");
    return;
  }
  const double scaled = value * 1000.0;
  const auto roundedScaled = static_cast<long long>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
  unsigned long long absScaled = static_cast<unsigned long long>(roundedScaled);
  if (roundedScaled < 0) {
    out->push_back('-');
    absScaled = static_cast<unsigned long long>(-roundedScaled);
  }
  AppendInteger(out, absScaled / 1000ULL);
  out->push_back('.');
  const auto fractional = static_cast<unsigned int>(absScaled % 1000ULL);
  out->push_back(static_cast<char>('0' + (fractional / 100U)));
  out->push_back(static_cast<char>('0' + ((fractional / 10U) % 10U)));
  out->push_back(static_cast<char>('0' + (fractional % 10U)));
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
  std::string line;
  line.reserve(256 + sessionId.size());

  AppendInteger(&line, unixMs);
  line.push_back(',');
  line.append(sessionId);
  line.push_back(',');
  AppendInteger(&line, targetFps);
  line.push_back(',');
  AppendInteger(&line, effectiveFps);
  line.push_back(',');
  line.append(DecodeModeToCsv(decodeMode));
  line.push_back(',');
  line.append(DecodePathToCsv(decodePath));
  line.push_back(',');
  AppendInteger(&line, metrics.decodeOutputPixels);
  line.push_back(',');
  line.append(RuntimeThreadQosToCsv(metrics.threadQos));
  line.push_back(',');
  line.push_back(metrics.occluded ? '1' : '0');
  line.push_back(',');
  line.append(RuntimePowerStateToCsv(metrics.powerState));
  line.push_back(',');
  AppendFixed3(&line, metrics.cpuPercent);
  line.push_back(',');
  AppendInteger(&line, metrics.privateBytes);
  line.push_back(',');
  AppendInteger(&line, metrics.workingSetBytes);
  line.push_back(',');
  AppendFixed3(&line, metrics.presentP95Ms);
  line.push_back(',');
  AppendInteger(&line, longRunLevel);
  line.push_back(',');
  AppendInteger(&line, decodeHotSleepMs);
  line.push_back(',');
  line.append(DecodeInteropStageToCsv(decodeInteropStage));
  line.push_back(',');
  AppendInteger(&line, decodeInteropHresult);
  line.push_back('\n');
  return line;
}

}  // namespace wallpaper
