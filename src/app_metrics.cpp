#include "wallpaper/app.h"

#include "wallpaper/decode_output_policy.h"
#include "wallpaper/metrics_log_line.h"
#include "wallpaper/playback_profile_policy.h"
#include "wallpaper/probe_cadence_policy.h"
#include "wallpaper/runtime_trim_policy.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#define PSAPI_VERSION 1
#include <windows.h>
#include <psapi.h>

namespace wallpaper {
namespace {

struct ProcessMemoryUsage final {
  std::size_t privateBytes = 0;
  std::size_t workingSetBytes = 0;
};

std::uint64_t FileTimeToU64(const FILETIME& ft) {
  return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32U) |
         static_cast<std::uint64_t>(ft.dwLowDateTime);
}

double QueryProcessCpuPercent() {
  static std::uint64_t lastProcess100ns = 0;
  static std::uint64_t lastWall100ns = 0;
  static DWORD cpuCount = 0;
  if (cpuCount == 0) {
    cpuCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (cpuCount == 0) {
      cpuCount = 1;
    }
  }

  FILETIME createFt{};
  FILETIME exitFt{};
  FILETIME kernelFt{};
  FILETIME userFt{};
  FILETIME nowFt{};
  if (!GetProcessTimes(GetCurrentProcess(), &createFt, &exitFt, &kernelFt, &userFt)) {
    return 0.0;
  }
  GetSystemTimeAsFileTime(&nowFt);

  const std::uint64_t process100ns = FileTimeToU64(kernelFt) + FileTimeToU64(userFt);
  const std::uint64_t wall100ns = FileTimeToU64(nowFt);

  if (lastProcess100ns == 0 || lastWall100ns == 0 || wall100ns <= lastWall100ns) {
    lastProcess100ns = process100ns;
    lastWall100ns = wall100ns;
    return 0.0;
  }

  const std::uint64_t deltaProc = process100ns - lastProcess100ns;
  const std::uint64_t deltaWall = wall100ns - lastWall100ns;
  lastProcess100ns = process100ns;
  lastWall100ns = wall100ns;
  if (deltaWall == 0) {
    return 0.0;
  }

  const double cpu =
      (static_cast<double>(deltaProc) / static_cast<double>(deltaWall)) * 100.0 /
      static_cast<double>(cpuCount);
  return std::clamp(cpu, 0.0, 100.0);
}

ProcessMemoryUsage QueryProcessMemoryUsage() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), counters.cb)) {
    return {};
  }
  ProcessMemoryUsage usage;
  usage.privateBytes = static_cast<std::size_t>(counters.PrivateUsage);
  usage.workingSetBytes = static_cast<std::size_t>(counters.WorkingSetSize);
  return usage;
}

bool TrimCurrentProcessWorkingSet() {
  return EmptyWorkingSet(GetCurrentProcess()) != FALSE;
}

double TakeP95Ms(std::vector<double>* values) {
  if (values == nullptr || values->empty()) {
    return 0.0;
  }
  const std::size_t n = values->size();
  const std::size_t index = ((n - 1) * 95) / 100;
  std::nth_element(values->begin(), values->begin() + static_cast<std::ptrdiff_t>(index),
                   values->end());
  const double p95 = (*values)[index];
  values->clear();
  return p95;
}

std::int64_t NowUnixMs() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

}  // namespace

void App::MaybeSampleAndLogMetrics(const bool attemptedRender, const bool frameDropped,
                                   const double presentMs) {
  if (attemptedRender) {
    ++totalFrames_;
    if (frameDropped) {
      ++droppedFrames_;
    } else if (presentMs > 0.0) {
      presentSamplesMs_.push_back(presentMs);
    }
  }

  const auto now = RenderScheduler::Clock::now();
  const bool hasActiveVideo = wallpaperAttached_ && decodeOpened_.load() && decodeRunning_.load();
  const bool occluded = wallpaperHost_ != nullptr && wallpaperHost_->IsOccluded();
  const auto metricsSampleInterval =
      SelectRuntimeMetricsSampleInterval(hasActiveVideo, stablePauseForLoopSleep_, occluded,
                                         lastDecodePath_, lastDecodeOutputPixels_,
                                         scheduler_.GetFpsCap());
  if (lastMetricsAt_ != RenderScheduler::Clock::time_point{} &&
      now - lastMetricsAt_ < metricsSampleInterval) {
    return;
  }
  lastMetricsAt_ = now;

  RuntimeMetrics metrics;
  metrics.decodeOutputPixels = lastDecodeOutputPixels_;
  metrics.threadQos = static_cast<RuntimeThreadQos>(decodeThreadQos_.load());
  metrics.occluded = occluded;
  metrics.powerState = arbiter_.CurrentPowerState();
  metrics.cpuPercent = QueryProcessCpuPercent();
  const ProcessMemoryUsage memoryUsage = QueryProcessMemoryUsage();
  metrics.privateBytes = memoryUsage.privateBytes;
  metrics.workingSetBytes = memoryUsage.workingSetBytes;
  metrics.presentP95Ms = TakeP95Ms(&presentSamplesMs_);
  metrics.droppedFrameRatio =
      totalFrames_ == 0 ? 0.0
                        : static_cast<double>(droppedFrames_) / static_cast<double>(totalFrames_);
  metrics_.PushSample(metrics);
  // 每秒基于实时负载做一次帧率档位决策，避免在每帧路径引入额外分支和抖动。
  const int effectiveFps = qualityGovernor_.Update(metrics);
  const LongRunLoadDecision longRunDecision =
      UpdateLongRunLoadPolicy(metrics, hasActiveVideo, stablePauseForLoopSleep_, lastDecodePath_,
                              &longRunLoadState_);
  decodePumpDynamicBoostMs_.store(longRunDecision.decodeHotSleepBoostMs);
  if (decodePipeline_ &&
      ShouldAllowLongRunDecodeTrimForPlaybackProfile(config_.playbackProfile) &&
      ShouldExecuteLongRunDecodeTrim(longRunDecision.requestDecodeTrim, decodeRunning_.load(),
                                     lastDecodePath_)) {
    decodePipeline_->TrimMemory();
  }
  if (ShouldAllowWorkingSetTrimForPlaybackProfile(config_.playbackProfile) &&
      ShouldRequestWorkingSetTrim(hasActiveVideo, lastDecodePath_, metrics.workingSetBytes,
                                  longRunLoadState_.level)) {
    const auto workingSetTrimInterval =
        SelectRuntimeWorkingSetTrimInterval(hasActiveVideo, lastDecodePath_, lastDecodeOutputPixels_,
                                            longRunLoadState_.level);
    if (workingSetTrimInterval.count() > 0 &&
        (lastWorkingSetTrimAt_ == RenderScheduler::Clock::time_point{} ||
         (now - lastWorkingSetTrimAt_) >= workingSetTrimInterval)) {
      TrimCurrentProcessWorkingSet();
      lastWorkingSetTrimAt_ = now;
    }
  }
  if (hasActiveVideo && IsCpuFallbackDecodePath(lastDecodePath_) &&
      ShouldAllowCpuFallbackRetuneForPlaybackProfile(config_.playbackProfile)) {
    const int desiredDecodeOpenLevel =
        SelectDecodeOpenLongRunLevel(longRunLoadState_.level, true, lastDecodeOutputPixels_);
    const bool desiredPreferHardwareTransforms =
        ResolvePreferHardwareTransformsForPlaybackProfile(
            config_.playbackProfile,
            ShouldPreferHardwareTransformsForDecodeOpen(desiredDecodeOpenLevel, true));
    if (ShouldReopenDecodeForLongRunTuning(
            true, lastDecodeOutputPixels_, decodeOpenLongRunLevel_, desiredDecodeOpenLevel,
            decodeOpenPreferHardwareTransforms_, desiredPreferHardwareTransforms)) {
      const int previousDecodeOpenLevel = decodeOpenLongRunLevel_;
      const bool previousPreferHardwareTransforms = decodeOpenPreferHardwareTransforms_;
      const std::string currentPath = config_.videoPath;
      if (ShouldActivateVideoPipelineCached(currentPath, false, now)) {
        decodePipeline_->Stop();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        const bool reopened = StartVideoPipelineForPath(
            currentPath, desiredDecodeOpenLevel, false, true, true,
            desiredPreferHardwareTransforms);
        if (!reopened) {
          StartVideoPipelineForPath(currentPath, previousDecodeOpenLevel, false, true, true,
                                    previousPreferHardwareTransforms);
        }
      }
    }
  }
  ApplyRenderFpsCap(effectiveFps);
  const int appliedFps = scheduler_.GetFpsCap();
  const int targetFps = autoTargetFps_;

  if (!metricsLogFile_.Append(BuildMetricsCsvLine(NowUnixMs(), metrics, metricsSessionId_,
                                                  targetFps, appliedFps, lastDecodeMode_,
                                                  lastDecodePath_, longRunLoadState_.level,
                                                  decodePumpHotSleepMs_.load(),
                                                  decodeCopyBytesInWindow_,
                                                  lastDecodeInteropStage_,
                                                  lastDecodeInteropHresult_))) {
    // I/O 失败时静默降级，避免主渲染循环被监控路径反向影响。
  }

  totalFrames_ = 0;
  droppedFrames_ = 0;
  decodeCopyBytesInWindow_ = 0;
}

}  // namespace wallpaper
