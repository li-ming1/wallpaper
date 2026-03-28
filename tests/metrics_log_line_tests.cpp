#include "wallpaper/metrics_log_line.h"

#include <cstdint>
#include <string>

#include "test_support.h"

TEST_CASE(MetricsLogLine_HeaderIncludesExtendedColumns) {
  const std::string header = wallpaper::BuildMetricsCsvHeader();
  EXPECT_EQ(header,
            "unix_ms,session_id,target_fps,effective_fps,adaptive_quality,decode_mode,cpu_percent,"
            "private_bytes,present_p95_ms,dropped_frame_ratio\n");
}

TEST_CASE(MetricsLogLine_EncodesAllFields) {
  wallpaper::RuntimeMetrics metrics;
  metrics.cpuPercent = 12.5;
  metrics.privateWorkingSetBytes = 4096;
  metrics.presentP95Ms = 4.25;
  metrics.droppedFrameRatio = 0.125;

  const std::string line =
      wallpaper::BuildMetricsCsvLine(123456789LL, metrics, "sess_1", 60, 30, true,
                                     wallpaper::DecodeMode::kMediaFoundation);

  EXPECT_EQ(line, "123456789,sess_1,60,30,1,mf,12.500,4096,4.250,0.125\n");
}

TEST_CASE(MetricsLogLine_EncodesUnknownDecodeMode) {
  wallpaper::RuntimeMetrics metrics;

  const std::string line =
      wallpaper::BuildMetricsCsvLine(1LL, metrics, "sess_2", 30, 30, false,
                                     wallpaper::DecodeMode::kUnknown);

  EXPECT_EQ(line, "1,sess_2,30,30,0,unknown,0.000,0,0.000,0.000\n");
}
