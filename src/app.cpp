#include "wallpaper/app.h"
#include "wallpaper/desktop_context_policy.h"
#include "wallpaper/decode_output_policy.h"
#include "wallpaper/decode_token_gate_policy.h"
#include "wallpaper/frame_bridge.h"
#include "wallpaper/long_run_load_policy.h"
#include "wallpaper/loop_sleep_policy.h"
#include "wallpaper/metrics_log_line.h"
#include "wallpaper/playback_profile_policy.h"
#include "app_autostart.h"
#include "wallpaper/pause_suspend_policy.h"
#include "wallpaper/pause_transition_policy.h"
#include "wallpaper/process_name_cache.h"
#include "wallpaper/probe_cadence_policy.h"
#include "wallpaper/runtime_trim_policy.h"
#include "wallpaper/startup_policy.h"
#include "wallpaper/upload_texture_policy.h"
#include "wallpaper/video_path_probe_policy.h"
#include "wallpaper/video_path_matcher.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

#define PSAPI_VERSION 1
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>

namespace wallpaper {
namespace {

std::wstring ExtractBaseName(std::wstring path) {
  const std::size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

bool TryQueryForegroundProcessBaseName(const HWND hwnd, std::wstring* outProcessName) {
  if (hwnd == nullptr || outProcessName == nullptr) {
    return false;
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == 0) {
    return false;
  }

  static ProcessNameCache processNameCache;
  switch (processNameCache.TryGet(processId, outProcessName)) {
    case ProcessNameCacheLookup::kHit:
      return true;
    case ProcessNameCacheLookup::kKnownFailure:
      return false;
    case ProcessNameCacheLookup::kMiss:
      break;
  }

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (process == nullptr) {
    processNameCache.RememberFailure(processId);
    return false;
  }

  wchar_t processPath[1024] = {};
  DWORD length = static_cast<DWORD>(std::size(processPath));
  const bool ok = QueryFullProcessImageNameW(process, 0, processPath, &length) != FALSE;
  CloseHandle(process);
  if (!ok || length == 0) {
    processNameCache.RememberFailure(processId);
    return false;
  }

  std::wstring processName = ExtractBaseName(std::wstring(processPath, length));
  processNameCache.RememberSuccess(processId, processName);
  *outProcessName = processName;
  return true;
}

std::uintptr_t QueryForegroundWindowHandle() {
  return reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
}

bool TryDetectDesktopContextActive(const std::uintptr_t foregroundWindowHandle, bool* outActive) {
  if (outActive == nullptr) {
    return false;
  }
  const HWND hwnd = reinterpret_cast<HWND>(foregroundWindowHandle);
  if (hwnd == nullptr) {
    return false;
  }

  wchar_t className[256] = {};
  if (GetClassNameW(hwnd, className, 255) == 0) {
    return false;
  }
  const std::wstring classNameValue(className);
  if (IsDesktopContextClass(classNameValue)) {
    *outActive = true;
    return true;
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId != 0 && processId == GetCurrentProcessId()) {
    *outActive = true;
    return true;
  }

  std::wstring processName;
  TryQueryForegroundProcessBaseName(hwnd, &processName);
  *outActive = ShouldTreatAsDesktopContext(classNameValue, processName);
  return true;
}

bool IsSessionInteractive() {
  // 锁屏/切换用户时通常无法获取输入桌面，借此快速判定会话不可交互。
  HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
  if (desktop == nullptr) {
    return false;
  }
  CloseDesktop(desktop);
  return true;
}

bool IsBatterySaverActive() {
  SYSTEM_POWER_STATUS powerStatus{};
  if (!GetSystemPowerStatus(&powerStatus)) {
    return false;
  }
  return powerStatus.SystemStatusFlag != 0;
}

bool IsRemoteSessionActive() { return GetSystemMetrics(SM_REMOTESESSION) != 0; }

bool TrimCurrentProcessWorkingSet() {
  return EmptyWorkingSet(GetCurrentProcess()) != FALSE;
}

bool SetCurrentProcessMemoryPriority(const ULONG priority) {
  MEMORY_PRIORITY_INFORMATION memoryInfo{};
  memoryInfo.MemoryPriority = priority;
  return SetProcessInformation(GetCurrentProcess(), ProcessMemoryPriority, &memoryInfo,
                               sizeof(memoryInfo)) != FALSE;
}

class ScopedHighResolutionTimer final {
 public:
  ScopedHighResolutionTimer() = default;

  ~ScopedHighResolutionTimer() { SetEnabled(false); }

  void SetEnabled(const bool enabled) {
    if (enabled == enabled_) {
      return;
    }
    if (enabled) {
      if (timeBeginPeriod(1) == TIMERR_NOERROR) {
        enabled_ = true;
      }
      return;
    }
    timeEndPeriod(1);
    enabled_ = false;
  }

 private:
  bool enabled_ = false;
};

void PumpThreadWindowMessages() {
  MSG msg{};
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

void WaitMainLoopInterval(const int sleepMs, const bool useMessageAwareWait,
                          void* const frameReadyEvent) {
  if (sleepMs <= 0) {
    return;
  }
  const DWORD waitMs = static_cast<DWORD>(std::clamp(sleepMs, 1, 500));
  const HANDLE decodeEvent = reinterpret_cast<HANDLE>(frameReadyEvent);
  if (useMessageAwareWait) {
    // 仅在长睡眠路径等待主线程消息，且忽略高频输入类消息，减少提前唤醒抖动。
    constexpr DWORD kMainLoopMessageMask = QS_POSTMESSAGE | QS_SENDMESSAGE | QS_TIMER;
    const DWORD handleCount = decodeEvent != nullptr ? 1U : 0U;
    HANDLE handles[1] = {decodeEvent};
    (void)MsgWaitForMultipleObjectsEx(handleCount, handleCount == 0 ? nullptr : handles, waitMs,
                                      kMainLoopMessageMask, MWMO_INPUTAVAILABLE);
    return;
  }
  if (decodeEvent != nullptr) {
    (void)WaitForSingleObjectEx(decodeEvent, waitMs, FALSE);
    return;
  }
  Sleep(waitMs);
}

std::int64_t NowUnixMs() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string BuildMetricsSessionId() {
  std::ostringstream out;
  out << "sess_" << NowUnixMs();
  out << '_' << static_cast<unsigned long>(GetCurrentProcessId());
  return out.str();
}

}  // namespace

App::App(std::filesystem::path configPath)
    : configWriter_(std::make_unique<AsyncFileWriter>(64)),
      metricsWriter_(std::make_unique<AsyncFileWriter>(256)),
      configStore_(configPath, configWriter_.get()),
      scheduler_(30),
      metricsLogFile_((configPath.has_parent_path() ? configPath.parent_path()
                                                    : std::filesystem::current_path()) /
                          "metrics.csv",
                      256U * 1024U,
                      BuildMetricsCsvHeader(),
                      7,
                      {},
                      metricsWriter_.get()),
      metrics_(300),
      qualityGovernor_(),
      metricsSessionId_(BuildMetricsSessionId()) {}

App::~App() {
  RequestStop();
  StopDecodePump();
  if (decodeFrameReadyEvent_ != nullptr) {
    CloseHandle(reinterpret_cast<HANDLE>(decodeFrameReadyEvent_));
    decodeFrameReadyEvent_ = nullptr;
  }
}

bool App::Initialize() {
  const bool configExistedBeforeLoad = configStore_.Exists();
  InvalidateVideoPathProbeCache();
  if (const auto loaded = configStore_.LoadExpected(); loaded.has_value()) {
    config_ = *loaded;
  } else {
    config_ = {};
  }
  RefreshAutoTargetFps(true);
  qualityGovernor_.SetEnabled(true);
  ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
  arbiter_.SetPauseWhenNotDesktopContext(config_.pauseWhenNotDesktopContext);

  wallpaperHost_ = CreateWallpaperHost();
  decodePipeline_ = CreateDecodePipeline();
  trayController_ = CreateTrayController();
  if (!wallpaperHost_ || !decodePipeline_ || !trayController_) {
    return false;
  }
  if (decodeFrameReadyEvent_ == nullptr) {
    decodeFrameReadyEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  }
  decodePipeline_->SetFrameReadyNotifier(&App::OnDecodeFrameReadyThunk, this);
  decodeFrameReadyNotifierAvailable_ = decodePipeline_->SupportsFrameReadyNotifier();

  const bool hasValidVideoPath = ShouldActivateVideoPipelineCached(
      config_.videoPath, false, RenderScheduler::Clock::now());
  const bool deferDecodeAtStartup =
      ShouldDeferVideoDecodeStart(configExistedBeforeLoad, hasValidVideoPath);
  if (hasValidVideoPath) {
    if (!StartVideoPipelineForPath(config_.videoPath, 0, true, !deferDecodeAtStartup)) {
      // 路径存在但启动失败时降级到“仅托盘运行”，避免出现不可控遮盖层。
      config_.videoPath.clear();
      DetachWallpaper();
    } else if (deferDecodeAtStartup) {
      startupDecodeDeferred_ = true;
      startupDecodeDeferredAt_ = RenderScheduler::Clock::now();
    }
  } else {
    // 缺失配置或路径失效时不附着壁纸窗口，避免幕布遮罩影响桌面体验。
    config_.videoPath.clear();
    DetachWallpaper();
  }

  SetAutoStartEnabled(config_.autoStart);
  lastMetricsAt_ = RenderScheduler::Clock::now();
  if (config_.debugMetrics && !metricsLogFile_.EnsureReady()) {
    // 指标日志写入失败不应阻止主功能启动，降级为无落盘监控。
  }
  SyncTrayMenuState();

  return true;
}

int App::Run() {
  ScopedHighResolutionTimer timerResolution;
  running_.store(true);
  StartDecodePump();
  trayController_->StartMessageLoop();

  while (running_.load()) {
    PumpThreadWindowMessages();

    if (trayController_->IsExitRequested()) {
      RequestStop();
      break;
    }
    if (!HandleTrayActions()) {
      break;
    }
    Tick();

    const bool shouldPause = stablePauseForLoopSleep_;
    const bool hasActiveVideo = wallpaperAttached_ && decodeOpened_.load() && decodeRunning_.load();
    const bool useHighResolutionTimer = ShouldUseHighResolutionTimer(
        hasActiveVideo, shouldPause, scheduler_.GetFpsCap(), longRunLoadState_.level,
        lastDecodePath_, decodeWarmupActive_.load());
    timerResolution.SetEnabled(useHighResolutionTimer);
    const auto untilNextRender = std::chrono::duration_cast<std::chrono::milliseconds>(
        scheduler_.TimeUntilNextRender(RenderScheduler::Clock::now()));
    const int sleepMs = ComputeMainLoopSleepMs(shouldPause, hasActiveVideo, untilNextRender);
    const bool useMessageAwareWait =
        ShouldUseMainLoopMessageAwareWait(shouldPause, hasActiveVideo);
    WaitMainLoopInterval(sleepMs, useMessageAwareWait, decodeFrameReadyEvent_);
  }

  StopDecodePump();
  if (decodePipeline_) {
    decodePipeline_->Stop();
    decodeRunning_.store(false);
    decodeOpened_.store(false);
  }
  DetachWallpaper();
  if (trayController_) {
    trayController_->StopMessageLoop();
  }
  return 0;
}

void App::RequestStop() { running_.store(false); }

void App::RefreshAutoTargetFps(const bool force) {
  const int nextTarget = ResolveAutoTargetFps(sourceFrameRateState_.sourceFps);
  if (!force && nextTarget == autoTargetFps_) {
    return;
  }
  autoTargetFps_ = nextTarget;
  qualityGovernor_.SetTargetFps(autoTargetFps_);
}

bool App::ShouldActivateVideoPipelineCached(const std::string& path, const bool allowCache,
                                            const RenderScheduler::Clock::time_point now) {
  if (path.empty()) {
    return false;
  }
  const auto cacheTtl = SelectVideoPathProbeCacheTtl(allowCache);
  const bool cacheInitialized =
      videoPathProbeCacheCheckedAt_ != RenderScheduler::Clock::time_point{};
  if (allowCache && ShouldUseCachedVideoPathProbe(path, videoPathProbeCachePath_, cacheInitialized,
                                                  now, videoPathProbeCacheCheckedAt_, cacheTtl)) {
    return videoPathProbeCacheValid_;
  }
  const bool active = ShouldActivateVideoPipeline(path);
  videoPathProbeCachePath_ = path;
  videoPathProbeCacheValid_ = active;
  videoPathProbeCacheCheckedAt_ = now;
  return active;
}

void App::InvalidateVideoPathProbeCache() {
  videoPathProbeCachePath_.clear();
  videoPathProbeCacheValid_ = false;
  videoPathProbeCacheCheckedAt_ = RenderScheduler::Clock::time_point{};
}

bool App::EnsureWallpaperAttached() {
  if (!wallpaperHost_) {
    return false;
  }
  if (wallpaperAttached_) {
    return true;
  }

  if (!wallpaperHost_->AttachToDesktop()) {
    return false;
  }
  wallpaperHost_->ResizeForDisplays();
  wallpaperAttached_ = true;
  return true;
}

void App::DetachWallpaper() {
  if (!wallpaperHost_ || !wallpaperAttached_) {
    frame_bridge::ClearLatestFrame();
    presentSamplesMs_.Clear();
    lastPresentedAt_ = RenderScheduler::Clock::time_point{};
    return;
  }
  wallpaperHost_->DetachFromDesktop();
  wallpaperAttached_ = false;
  frame_bridge::ClearLatestFrame();
  presentSamplesMs_.Clear();
  lastPresentedAt_ = RenderScheduler::Clock::time_point{};
}

void App::ResetPlaybackState(const bool resetLongRunState) {
  hasLastPresentedFrame_ = false;
  lastPresentedAt_ = RenderScheduler::Clock::time_point{};
  syntheticSequence_ = 0;
  lastDecodedTimestamp100ns_ = -1;
  ResetSourceFrameRateState(&sourceFrameRateState_);
  RefreshAutoTargetFps(true);
  trayMenuVisible_ = false;
  lastTrayInteractionAt_ = RenderScheduler::Clock::time_point{};
  lastDecodeMode_ = DecodeMode::kUnknown;
  lastDecodePath_ = DecodePath::kUnknown;
  lastDecodeInteropStage_ = DecodeInteropStage::kUnknown;
  lastDecodeInteropHresult_ = 0;
  lastDecodeOutputPixels_ = 0;
  lastSessionProbeAt_ = RenderScheduler::Clock::time_point{};
  lastForegroundProbeAt_ = RenderScheduler::Clock::time_point{};
  lastForegroundDeepProbeAt_ = RenderScheduler::Clock::time_point{};
  lastForegroundWindowHandle_ = 0;
  foregroundProbeFailureStreak_ = 0;
  cachedSessionInteractive_ = true;
  cachedDesktopContextActive_ = true;
  cachedBatterySaverActive_ = false;
  cachedRemoteSessionActive_ = false;
  if (resetLongRunState) {
    longRunLoadState_ = LongRunLoadState{};
  }
  stablePauseForLoopSleep_ = false;
  wasPaused_ = false;
  decodeCacheTrimmedByPause_ = false;
  resourcesReleasedByPause_ = false;
  resumePipelinePending_ = false;
  nextResumeAttemptAt_ = RenderScheduler::Clock::time_point{};
  resumeWarmupOpened_ = false;
  resumeWarmupStarted_ = false;
  nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
  warmResumeRetryFailures_ = 0;
  resumePipelineRetryFailures_ = 0;
  startupDecodeDeferred_ = false;
  startupDecodeDeferredAt_ = RenderScheduler::Clock::time_point{};
  pauseEnteredAt_ = RenderScheduler::Clock::time_point{};
  lastWorkingSetTrimAt_ = RenderScheduler::Clock::time_point{};
  startupWorkingSetTrimDone_ = false;
  hardSuspendedByPause_ = false;
  pauseTransitionState_ = PauseTransitionState{};
  decodePumpDynamicBoostMs_.store(0);
  decodeThreadQos_.store(static_cast<int>(RuntimeThreadQos::kNormal));
  decodeWarmupActive_.store(false);
  decodeOpenLongRunLevel_ = 0;
  decodeOpenPreferHardwareTransforms_ = true;
  {
    std::lock_guard<std::mutex> lock(decodedTokenMu_);
    hasLatestDecodedToken_ = false;
    latestDecodedToken_ = FrameToken{};
  }
  latestDecodedSequence_.store(0, std::memory_order_release);
  latestPresentedSequence_.store(0, std::memory_order_release);
}

bool App::StartVideoPipelineForPath(const std::string& path, const int longRunLoadLevel,
                                    const bool resetLongRunState,
                                    const bool startDecodeImmediately,
                                    const bool allowCachedPathProbe,
                                    const bool preferHardwareTransforms) {
  if (!decodePipeline_) {
    return false;
  }
  const auto now = RenderScheduler::Clock::now();
  if (!ShouldActivateVideoPipelineCached(path, allowCachedPathProbe, now)) {
    return false;
  }
  if (!EnsureWallpaperAttached()) {
    return false;
  }
  DecodeOpenProfile openProfile;
  openProfile.longRunLoadLevel = longRunLoadLevel;
  const bool effectivePreferHardwareTransforms =
      ResolvePreferHardwareTransformsForPlaybackProfile(config_.playbackProfile,
                                                        preferHardwareTransforms);
  openProfile.preferHardwareTransforms = effectivePreferHardwareTransforms;
  openProfile.requireHardwareTransforms = false;
  if (!decodePipeline_->Open(path, openProfile)) {
    decodeOpened_.store(false);
    decodeRunning_.store(false);
    decodeFrameReadyNotifierAvailable_ = false;
    return false;
  }
  decodeFrameReadyNotifierAvailable_ = decodePipeline_->SupportsFrameReadyNotifier();
  decodeOpened_.store(true);
  if (startDecodeImmediately) {
    if (!decodePipeline_->Start()) {
      decodePipeline_->Stop();
      decodeOpened_.store(false);
      decodeRunning_.store(false);
      return false;
    }
    decodeRunning_.store(true);
    startupDecodeDeferred_ = false;
    startupDecodeDeferredAt_ = RenderScheduler::Clock::time_point{};
  } else {
    decodeRunning_.store(false);
  }
  ResetPlaybackState(resetLongRunState);
  decodeOpenLongRunLevel_ = longRunLoadLevel;
  decodeOpenPreferHardwareTransforms_ = effectivePreferHardwareTransforms;
  if (startDecodeImmediately) {
    WakeDecodePump();
  }
  ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
  scheduler_.Reset();
  return true;
}

void App::ApplyRenderFpsCap(const int governorFps) {
  int desired = NormalizeFpsCap(governorFps);
  desired = ClampRenderFpsForCompactCpuFallback(desired, lastDecodePath_, lastDecodeOutputPixels_);
  int baseHotSleepMs = ComputeDecodePumpHotSleepMs(desired, sourceFrameRateState_.sourceFps);
  int dynamicBoostMs = decodePumpDynamicBoostMs_.load();
  if (IsCpuFallbackDecodePath(lastDecodePath_)) {
    // CPU-only 回退链路下进一步放缓解码拉帧频率，降低长期动态 CPU 压力。
    baseHotSleepMs += (desired >= 60 ? 10 : 14);
    if (dynamicBoostMs > 0) {
      dynamicBoostMs += 8;
    }
  }
  const int requestedHotSleepMs = std::clamp(baseHotSleepMs + dynamicBoostMs, 6, 64);
  const int nextHotSleepMs =
      ClampDecodePumpHotSleepForRealtime(requestedHotSleepMs, desired, sourceFrameRateState_.sourceFps);
  const int previousHotSleepMs = decodePumpHotSleepMs_.load();
  const int previousFpsCap = scheduler_.GetFpsCap();

  if (previousHotSleepMs != nextHotSleepMs) {
    decodePumpHotSleepMs_.store(nextHotSleepMs);
  }
  if (previousFpsCap != desired) {
    scheduler_.SetFpsCap(desired);
  }
  if (ShouldWakeDecodePumpForRenderCapUpdate(previousHotSleepMs, nextHotSleepMs, previousFpsCap,
                                             desired)) {
    WakeDecodePump();
  }
}

void App::Tick() {
  const auto applyProcessMemoryPriority = [this](const bool aggressive) {
    const ULONG desiredPriority = aggressive ? MEMORY_PRIORITY_VERY_LOW : MEMORY_PRIORITY_NORMAL;
    if (processMemoryPriority_ != desiredPriority &&
        SetCurrentProcessMemoryPriority(desiredPriority)) {
      processMemoryPriority_ = desiredPriority;
    }
  };

  if (!decodePipeline_ || !wallpaperHost_ || !wallpaperAttached_) {
    applyProcessMemoryPriority(false);
    stablePauseForLoopSleep_ = false;
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  const RuntimeProbeIntervals probeIntervals = SelectRuntimeProbeIntervals(wasPaused_);
  const auto now = RenderScheduler::Clock::now();
  const auto sessionProbeInterval = SelectSessionProbeIntervalForState(
      probeIntervals.session, cachedSessionInteractive_, cachedBatterySaverActive_,
      cachedRemoteSessionActive_);
  constexpr std::chrono::milliseconds kTrayInteractionProbeFreeze(1200);
  const bool inTrayInteractionFreeze =
      lastTrayInteractionAt_ != RenderScheduler::Clock::time_point{} &&
      now >= lastTrayInteractionAt_ &&
      (now - lastTrayInteractionAt_) <= kTrayInteractionProbeFreeze;
  const bool suppressDesktopContextProbe = trayMenuVisible_ || inTrayInteractionFreeze;
  if (ShouldRefreshRuntimeProbe(now, lastSessionProbeAt_, sessionProbeInterval)) {
    cachedSessionInteractive_ = IsSessionInteractive();
    cachedBatterySaverActive_ = IsBatterySaverActive();
    cachedRemoteSessionActive_ = IsRemoteSessionActive();
    lastSessionProbeAt_ = now;
  }
  const bool shouldProbeForeground =
      ShouldRefreshRuntimeProbe(now, lastForegroundProbeAt_, probeIntervals.foreground);
  if (shouldProbeForeground && !suppressDesktopContextProbe) {
    constexpr std::chrono::milliseconds kForegroundDeepProbeReuseInterval(1200);
    const std::uintptr_t foregroundWindowHandle = QueryForegroundWindowHandle();
    if (ShouldReuseForegroundProbeResult(foregroundWindowHandle, lastForegroundWindowHandle_, now,
                                         lastForegroundDeepProbeAt_,
                                         kForegroundDeepProbeReuseInterval)) {
      // 前台窗口未变化时复用最近深度探测结果，减少高开销进程查询。
      lastForegroundProbeAt_ = now;
    } else {
      bool desktopContextActive = cachedDesktopContextActive_;
      const bool probeSucceeded =
          TryDetectDesktopContextActive(foregroundWindowHandle, &desktopContextActive);
      foregroundProbeFailureStreak_ =
          UpdateForegroundProbeFailureStreak(probeSucceeded, foregroundProbeFailureStreak_);
      if (probeSucceeded) {
        cachedDesktopContextActive_ = desktopContextActive;
        lastForegroundProbeAt_ = now;
        lastForegroundDeepProbeAt_ = now;
        lastForegroundWindowHandle_ = foregroundWindowHandle;
      } else {
        constexpr int kForegroundProbeFailureFallbackThreshold = 3;
        if (ShouldUseConservativeDesktopContext(foregroundProbeFailureStreak_,
                                                kForegroundProbeFailureFallbackThreshold)) {
          // 前台探测连续失败时采用保守降载策略，避免非桌面启动阶段长时间保持动态。
          cachedDesktopContextActive_ = false;
          lastForegroundProbeAt_ = now;
          lastForegroundDeepProbeAt_ = now;
          lastForegroundWindowHandle_ = foregroundWindowHandle;
        }
      }
    }
  }
  arbiter_.SetSessionActive(cachedSessionInteractive_);
  arbiter_.SetDesktopContextActive(cachedDesktopContextActive_);
  arbiter_.SetDesktopVisible(!(wallpaperHost_ != nullptr && wallpaperHost_->IsOccluded()));
  arbiter_.SetBatterySaverActive(cachedBatterySaverActive_);
  arbiter_.SetRemoteSessionActive(cachedRemoteSessionActive_);
  const bool rawShouldPause = arbiter_.ShouldPause();
  constexpr std::chrono::milliseconds kPauseEnterDelay(110);
  constexpr std::chrono::milliseconds kPauseExitDelay(180);
  constexpr std::chrono::milliseconds kPauseExitDelayWarmup(360);
  const PauseSuspendThresholdProfile pauseSuspendProfile =
      DefaultPauseSuspendThresholdProfile();
  const auto effectivePauseExitDelay =
      hardSuspendedByPause_ ? kPauseExitDelayWarmup : kPauseExitDelay;
  const bool shouldPause =
      UpdatePauseTransition(rawShouldPause, now, kPauseEnterDelay, effectivePauseExitDelay,
                            &pauseTransitionState_);
  stablePauseForLoopSleep_ = shouldPause;

  if (shouldPause) {
    applyProcessMemoryPriority(false);
    if (!wasPaused_) {
      // 先做轻量暂停，保留解码上下文，减少短时切换的恢复顿挫。
      if (decodeRunning_.load()) {
        decodePipeline_->Pause();
        decodeRunning_.store(false);
      }
      resourcesReleasedByPause_ = false;
      hardSuspendedByPause_ = false;
      decodeCacheTrimmedByPause_ = false;
      resumeWarmupOpened_ = false;
      resumeWarmupStarted_ = false;
      decodeWarmupActive_.store(false);
      nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
      warmResumeRetryFailures_ = 0;
      resumePipelineRetryFailures_ = 0;
      pauseEnteredAt_ = now;
      scheduler_.Reset();
      // 保留最后一帧并仅停止解码，让切换到“静态壁纸”时更自然。
      presentSamplesMs_.Clear();
      wasPaused_ = true;
    } else if (!hardSuspendedByPause_ && decodeOpened_.load()) {
      const auto pausedDuration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - pauseEnteredAt_);
      if (!decodeCacheTrimmedByPause_ &&
          ShouldTrimDecodeCacheDuringPause(pausedDuration,
                                           pauseSuspendProfile.trimDecodeCacheAfter)) {
        // 轻暂停持续一段时间后释放 CPU 侧帧缓存，降低内存驻留峰值。
        decodePipeline_->TrimMemory();
        decodeCacheTrimmedByPause_ = true;
      }
      const bool allowAggressiveSuspend = arbiter_.ShouldAllowHardSuspend();
      const auto hardSuspendThreshold =
          SelectHardSuspendThreshold(allowAggressiveSuspend,
                                     pauseSuspendProfile.hardSuspendAggressive,
                                     pauseSuspendProfile.hardSuspendConservative);
      if (ShouldHardSuspendDuringPause(pausedDuration, hardSuspendThreshold)) {
        // 长暂停再升级为硬挂起，兼顾资源回收与短切换流畅性。
        decodePipeline_->Stop();
        TrimCurrentProcessWorkingSet();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        hardSuspendedByPause_ = true;
        resumeWarmupOpened_ = false;
        resumeWarmupStarted_ = false;
        decodeWarmupActive_.store(false);
        nextWarmupAttemptAt_ = now;
        resourcesReleasedByPause_ = true;
      }
    } else if (hardSuspendedByPause_) {
      const bool shouldWarmResume = ShouldWarmResumeDuringPause(rawShouldPause, hardSuspendedByPause_);
      if (shouldWarmResume && !resumeWarmupOpened_ && now >= nextWarmupAttemptAt_) {
        if (ShouldActivateVideoPipelineCached(config_.videoPath, true, now) &&
            decodePipeline_->Open(config_.videoPath,
                                  DecodeOpenProfile{decodeOpenLongRunLevel_,
                                                    decodeOpenPreferHardwareTransforms_, false})) {
          // 在退出 pause 迟滞窗口内预热 Open，恢复时只需 Start，降低解冻卡顿。
          decodeOpened_.store(true);
          decodeRunning_.store(false);
          resumeWarmupOpened_ = true;
          resumeWarmupStarted_ = false;
          decodeWarmupActive_.store(true);
          warmResumeRetryFailures_ = 0;
          nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
        } else {
          ++warmResumeRetryFailures_;
          nextWarmupAttemptAt_ = now + ComputeWarmResumeRetryDelay(warmResumeRetryFailures_);
        }
      } else if (shouldWarmResume && resumeWarmupOpened_ && !resumeWarmupStarted_) {
        if (decodePipeline_->Start()) {
          decodeRunning_.store(true);
          resumeWarmupStarted_ = true;
          decodeWarmupActive_.store(true);
          warmResumeRetryFailures_ = 0;
          WakeDecodePump();
        } else {
          decodePipeline_->Stop();
          decodeOpened_.store(false);
          decodeRunning_.store(false);
          resumeWarmupOpened_ = false;
          resumeWarmupStarted_ = false;
          decodeWarmupActive_.store(false);
          ++warmResumeRetryFailures_;
          nextWarmupAttemptAt_ = now + ComputeWarmResumeRetryDelay(warmResumeRetryFailures_);
        }
      } else if (!shouldWarmResume && resumeWarmupOpened_) {
        // 预热后又回到暂停态时回收预热资源，避免频繁切换导致内存反复抬升。
        decodePipeline_->Stop();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        resumeWarmupOpened_ = false;
        resumeWarmupStarted_ = false;
        decodeWarmupActive_.store(false);
        warmResumeRetryFailures_ = 0;
        nextWarmupAttemptAt_ = now + ComputeWarmResumeRetryDelay(1);
      }
    }
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  if (wasPaused_) {
    // 从 pause 恢复后重置调度，让 ShouldRender() 立即放行首帧，减少恢复黑屏/静止时间。
    scheduler_.Reset();
    if (hardSuspendedByPause_) {
      if (resumeWarmupOpened_ && decodeOpened_.load()) {
        if (!resumeWarmupStarted_ && !decodePipeline_->Start()) {
          decodePipeline_->Stop();
          decodeOpened_.store(false);
          decodeRunning_.store(false);
          resumePipelinePending_ = ShouldActivateVideoPipelineCached(config_.videoPath, true, now);
          ++resumePipelineRetryFailures_;
          nextResumeAttemptAt_ = now;
        } else {
          decodeRunning_.store(true);
          resumeWarmupStarted_ = true;
          decodeWarmupActive_.store(true);
          resumePipelineRetryFailures_ = 0;
        }
      } else {
        resumePipelinePending_ = ShouldActivateVideoPipelineCached(config_.videoPath, true, now);
        resumePipelineRetryFailures_ = 0;
        nextResumeAttemptAt_ = now;
      }
    } else if (decodeOpened_.load() && !decodeRunning_.load()) {
      const bool resumed = decodePipeline_->Start();
      decodeRunning_.store(resumed);
      if (resumed) {
        decodeWarmupActive_.store(false);
        WakeDecodePump();
      }
    }
    resourcesReleasedByPause_ = false;
    hardSuspendedByPause_ = false;
    decodeCacheTrimmedByPause_ = false;
    resumeWarmupOpened_ = false;
    resumeWarmupStarted_ = false;
    decodeWarmupActive_.store(false);
    nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
    pauseEnteredAt_ = RenderScheduler::Clock::time_point{};
    wasPaused_ = false;
  }

  if (resumePipelinePending_ && now >= nextResumeAttemptAt_) {
    if (StartVideoPipelineForPath(config_.videoPath, decodeOpenLongRunLevel_, false, true, true,
                                  decodeOpenPreferHardwareTransforms_)) {
      resumePipelinePending_ = false;
      resumePipelineRetryFailures_ = 0;
      nextResumeAttemptAt_ = RenderScheduler::Clock::time_point{};
    } else {
      ++resumePipelineRetryFailures_;
      nextResumeAttemptAt_ =
          now + ComputeResumePipelineRetryDelay(resumePipelineRetryFailures_);
    }
  }

  if (decodeOpened_.load() && !decodeRunning_.load()) {
    constexpr std::chrono::milliseconds kStartupDecodeDeferWindow(2500);
    const std::chrono::milliseconds deferredElapsed =
        startupDecodeDeferred_ && startupDecodeDeferredAt_ != RenderScheduler::Clock::time_point{} &&
                now >= startupDecodeDeferredAt_
            ? std::chrono::duration_cast<std::chrono::milliseconds>(now - startupDecodeDeferredAt_)
            : std::chrono::milliseconds(0);
    if (ShouldStartDeferredDecodeNow(startupDecodeDeferred_, deferredElapsed,
                                     kStartupDecodeDeferWindow)) {
      const bool resumed = decodePipeline_->Start();
      decodeRunning_.store(resumed);
      if (resumed) {
        decodeWarmupActive_.store(false);
        startupDecodeDeferred_ = false;
        startupDecodeDeferredAt_ = RenderScheduler::Clock::time_point{};
        WakeDecodePump();
      }
    }
  }

  const auto activeWorkingSetTrimInterval = SelectActiveWorkingSetTrimInterval(
      wallpaperAttached_ && decodeOpened_.load() && decodeRunning_.load(), lastDecodePath_,
      lastDecodeOutputPixels_);
  applyProcessMemoryPriority(ShouldUseAggressiveMemoryPriority(
      wallpaperAttached_ && decodeOpened_.load() && decodeRunning_.load(), lastDecodePath_,
      lastDecodeOutputPixels_));
  if (activeWorkingSetTrimInterval.count() > 0 &&
      (lastWorkingSetTrimAt_ == RenderScheduler::Clock::time_point{} ||
       (now - lastWorkingSetTrimAt_) >= activeWorkingSetTrimInterval)) {
    TrimCurrentProcessWorkingSet();
    lastWorkingSetTrimAt_ = now;
  }

  if (!scheduler_.ShouldRender(RenderScheduler::Clock::now())) {
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  FrameToken frame{};
  bool hasNewDecodedToken = false;
  const std::uint64_t latestDecodedSequence =
      latestDecodedSequence_.load(std::memory_order_acquire);
  if (ShouldAttemptDecodedTokenConsume(
          hasLastPresentedFrame_, hasLastPresentedFrame_ ? lastPresentedFrame_.sequence : 0,
          latestDecodedSequence)) {
    std::lock_guard<std::mutex> lock(decodedTokenMu_);
    if (hasLatestDecodedToken_) {
      const bool sequenceAdvanced =
          !hasLastPresentedFrame_ || latestDecodedToken_.sequence != lastPresentedFrame_.sequence;
      if (sequenceAdvanced) {
        frame = latestDecodedToken_;
        hasNewDecodedToken = true;
      }
    }
  }

  if (hasNewDecodedToken) {
    lastDecodePath_ = frame.decodePath;
    if (config_.debugMetrics) {
      lastDecodeMode_ = frame.decodeMode;
      lastDecodeInteropStage_ = frame.decodeInteropStage;
      lastDecodeInteropHresult_ = frame.decodeInteropHresult;
    }
    if (frame.width > 0 && frame.height > 0) {
      lastDecodeOutputPixels_ =
          static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    }
    if (frame.decodeMode == DecodeMode::kMediaFoundation) {
      const int observedSourceFps =
          UpdateSourceFrameRateState(lastDecodedTimestamp100ns_, frame.timestamp100ns,
                                     &sourceFrameRateState_);
      (void)observedSourceFps;
      RefreshAutoTargetFps(false);
      lastDecodedTimestamp100ns_ = frame.timestamp100ns;
    } else {
      ResetSourceFrameRateState(&sourceFrameRateState_);
      lastDecodedTimestamp100ns_ = -1;
      RefreshAutoTargetFps(true);
    }
    if (ShouldExecuteStartupWorkingSetTrim(
            wallpaperAttached_ && decodeOpened_.load() && decodeRunning_.load(), frame.decodePath,
            lastDecodeOutputPixels_, frame.gpuBacked, startupWorkingSetTrimDone_)) {
      // 共享 GPU NV12 桥接把大页驻留留在 present 之后更容易命中活跃窗口，
      // 首帧到达时预付一次 trim，并同步重置节奏基线，减少后续 CPU 尖峰数量。
      TrimCurrentProcessWorkingSet();
      lastWorkingSetTrimAt_ = now;
      startupWorkingSetTrimDone_ = true;
    }
    ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
    lastPresentedFrame_ = frame;
    hasLastPresentedFrame_ = true;
  } else if (hasLastPresentedFrame_) {
    frame = lastPresentedFrame_;
  } else {
    // 首帧未就绪时不做任何呈现，彻底消除启动瞬间回退底色“幕布”。
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  if (!ShouldPresentFrame(hasNewDecodedToken)) {
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  const auto presentBegin = RenderScheduler::Clock::now();
  wallpaperHost_->Present(frame);
  const auto presentEnd = RenderScheduler::Clock::now();
  if (hasNewDecodedToken) {
    // 只有 Present 真正返回后，才允许解码泵把该序号视作“已完成消费”。
    latestPresentedSequence_.store(frame.sequence, std::memory_order_release);
  }
  lastPresentedAt_ = presentEnd;
  const double presentMs =
      std::chrono::duration<double, std::milli>(presentEnd - presentBegin).count();
  MaybeSampleAndLogMetrics(true, false, presentMs);
}

}  // namespace wallpaper
