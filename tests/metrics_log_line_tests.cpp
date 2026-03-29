#include "wallpaper/metrics_log_line.h"

#include <cstdint>
#include <string>

#include "test_support.h"

TEST_CASE(MetricsLogLine_HeaderIncludesExtendedColumns) {
  const std::string header = wallpaper::BuildMetricsCsvHeader();
  EXPECT_EQ(header,
            "unix_ms,session_id,target_fps,effective_fps,adaptive_quality,decode_mode,decode_path,"
            "decode_output_pixels,thread_qos,occluded,power_state,"
            "cpu_percent,private_bytes,working_set_bytes,present_p95_ms,dropped_frame_ratio,"
            "long_run_level,decode_hot_sleep_ms,decode_copy_bytes_per_sec\n");
}

TEST_CASE(MetricsLogLine_EncodesAllFields) {
  wallpaper::RuntimeMetrics metrics;
  metrics.decodeOutputPixels = 921600;
  metrics.threadQos = wallpaper::RuntimeThreadQos::kEco;
  metrics.occluded = true;
  metrics.powerState = wallpaper::RuntimePowerState::kBatterySaver;
  metrics.cpuPercent = 12.5;
  metrics.privateBytes = 4096;
  metrics.workingSetBytes = 8192;
  metrics.presentP95Ms = 4.25;
  metrics.droppedFrameRatio = 0.125;

  const std::string line =
      wallpaper::BuildMetricsCsvLine(123456789LL, metrics, "sess_1", 60, 30, true,
                                     wallpaper::DecodeMode::kMediaFoundation,
                                     wallpaper::DecodePath::kDxvaZeroCopy, 2, 24, 1024);

  EXPECT_EQ(line,
            "123456789,sess_1,60,30,1,mf,dxva_zero_copy,921600,eco,1,battery_saver,12.500,"
            "4096,8192,4.250,0.125,2,24,1024\n");
}

TEST_CASE(MetricsLogLine_EncodesUnknownDecodeMode) {
  wallpaper::RuntimeMetrics metrics;
  metrics.threadQos = wallpaper::RuntimeThreadQos::kNormal;
  metrics.powerState = wallpaper::RuntimePowerState::kNormal;

  const std::string line =
      wallpaper::BuildMetricsCsvLine(1LL, metrics, "sess_2", 30, 30, false,
                                     wallpaper::DecodeMode::kUnknown,
                                     wallpaper::DecodePath::kUnknown, 0, 0, 0);

  EXPECT_EQ(line,
            "1,sess_2,30,30,0,unknown,unknown,0,normal,0,normal,0.000,0,0,0.000,0.000,0,0,0\n");
}

TEST_CASE(MetricsLogLine_EncodesCpuNv12FallbackPath) {
  wallpaper::RuntimeMetrics metrics;
  metrics.threadQos = wallpaper::RuntimeThreadQos::kEco;
  metrics.powerState = wallpaper::RuntimePowerState::kNormal;

  const std::string line =
      wallpaper::BuildMetricsCsvLine(2LL, metrics, "sess_3", 30, 30, true,
                                     wallpaper::DecodeMode::kMediaFoundation,
                                     wallpaper::DecodePath::kCpuNv12Fallback, 1, 36, 256);

  EXPECT_EQ(line,
            "2,sess_3,30,30,1,mf,cpu_nv12_fallback,0,eco,0,normal,0.000,0,0,0.000,0.000,1,36,256\n");
}
