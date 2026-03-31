#include "wallpaper/app.h"
#include "wallpaper/desktop_context_policy.h"
#include "wallpaper/decode_token_gate_policy.h"
#include "wallpaper/frame_bridge.h"
#include "wallpaper/long_run_load_policy.h"
#include "wallpaper/loop_sleep_policy.h"
#include "wallpaper/metrics_log_line.h"
#include "wallpaper/pause_suspend_policy.h"
#include "wallpaper/pause_transition_policy.h"
#include "wallpaper/probe_cadence_policy.h"
#include "wallpaper/runtime_trim_policy.h"
#include "wallpaper/startup_policy.h"
#include "wallpaper/video_path_probe_policy.h"
#include "wallpaper/video_path_matcher.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define PSAPI_VERSION 1
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#endif

namespace wallpaper {
namespace {

#ifdef _WIN32
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

  static DWORD lastProcessId = 0;
  static bool lastLookupOk = false;
  static std::wstring lastProcessName;
  if (processId == lastProcessId) {
    if (!lastLookupOk) {
      return false;
    }
    *outProcessName = lastProcessName;
    return true;
  }

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (process == nullptr) {
    lastProcessId = processId;
    lastLookupOk = false;
    lastProcessName.clear();
    return false;
  }

  wchar_t processPath[1024] = {};
  DWORD length = static_cast<DWORD>(std::size(processPath));
  const bool ok = QueryFullProcessImageNameW(process, 0, processPath, &length) != FALSE;
  CloseHandle(process);
  if (!ok || length == 0) {
    lastProcessId = processId;
    lastLookupOk = false;
    lastProcessName.clear();
    return false;
  }

  lastProcessId = processId;
  lastLookupOk = true;
  lastProcessName = ExtractBaseName(std::wstring(processPath, length));
  *outProcessName = lastProcessName;
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

std::uint64_t FileTimeToU64(const FILETIME& ft) {
  return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32U) |
         static_cast<std::uint64_t>(ft.dwLowDateTime);
}

struct ProcessMemoryUsage final {
  std::size_t privateBytes = 0;
  std::size_t workingSetBytes = 0;
};

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

bool SetAutoStartEnabled(const bool enabled) {
  constexpr wchar_t kRunPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  constexpr wchar_t kValueName[] = L"WallpaperDynamicDesktop";

  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return false;
  }

  bool ok = false;
  if (enabled) {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
      const std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
      const BYTE* bytes = reinterpret_cast<const BYTE*>(cmd.c_str());
      const DWORD byteSize = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
      ok = RegSetValueExW(key, kValueName, 0, REG_SZ, bytes, byteSize) == ERROR_SUCCESS;
    }
  } else {
    const LONG rc = RegDeleteValueW(key, kValueName);
    ok = (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
  }

  RegCloseKey(key);
  return ok;
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
#else
double QueryProcessCpuPercent() { return 0.0; }
ProcessMemoryUsage QueryProcessMemoryUsage() { return {}; }
bool IsSessionInteractive() { return true; }
bool IsBatterySaverActive() { return false; }
bool IsRemoteSessionActive() { return false; }
bool TrimCurrentProcessWorkingSet() { return false; }
bool SetAutoStartEnabled(bool) { return true; }
std::uintptr_t QueryForegroundWindowHandle() { return 0; }
bool TryDetectDesktopContextActive(std::uintptr_t, bool* outActive) {
  if (outActive == nullptr) {
    return false;
  }
  *outActive = true;
  return true;
}
#endif

#ifndef _WIN32
class ScopedHighResolutionTimer final {
 public:
  ScopedHighResolutionTimer() = default;
  void SetEnabled(bool) {}
};
#endif

void PumpThreadWindowMessages() {
#ifdef _WIN32
  MSG msg{};
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
#endif
}

void WaitMainLoopInterval(const int sleepMs, const bool useMessageAwareWait) {
  if (sleepMs <= 0) {
    return;
  }
#ifdef _WIN32
  const DWORD waitMs = static_cast<DWORD>(std::clamp(sleepMs, 1, 500));
  if (useMessageAwareWait) {
    // 仅在长睡眠路径等待主线程消息，且忽略高频输入类消息，减少提前唤醒抖动。
    constexpr DWORD kMainLoopMessageMask = QS_POSTMESSAGE | QS_SENDMESSAGE | QS_TIMER;
    (void)MsgWaitForMultipleObjectsEx(0, nullptr, waitMs, kMainLoopMessageMask,
                                      MWMO_INPUTAVAILABLE);
    return;
  }
  Sleep(waitMs);
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
#endif
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

std::string BuildMetricsSessionId() {
  std::ostringstream out;
  out << "sess_" << NowUnixMs();
#ifdef _WIN32
  out << '_' << static_cast<unsigned long>(GetCurrentProcessId());
#endif
  return out.str();
}

}  // namespace

App::App(std::filesystem::path configPath)
    : configStore_(configPath),
      scheduler_(30),
      metricsLogFile_((configPath.has_parent_path() ? configPath.parent_path()
                                                    : std::filesystem::current_path()) /
                          "metrics.csv",
                      256U * 1024U,
                      BuildMetricsCsvHeader(),
                      7),
      metrics_(300),
      qualityGovernor_(),
      metricsSessionId_(BuildMetricsSessionId()) {
  // 指标窗口为固定低量样本，预留容量可降低反复扩容带来的堆分配与碎片风险。
  presentSamplesMs_.reserve(128);
}

App::~App() {
  RequestStop();
  StopDecodePump();
}

bool App::Initialize() {
  const bool configExistedBeforeLoad = configStore_.Exists();
  InvalidateVideoPathProbeCache();
  if (const auto loaded = configStore_.LoadExpected(); loaded.has_value()) {
    config_ = *loaded;
  } else {
    config_ = {};
  }
  qualityGovernor_.SetTargetFps(config_.fpsCap);
  qualityGovernor_.SetEnabled(config_.adaptiveQuality);
  ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
  arbiter_.SetPauseWhenNotDesktopContext(config_.pauseWhenNotDesktopContext);

  wallpaperHost_ = CreateWallpaperHost();
  decodePipeline_ = CreateDecodePipeline();
  trayController_ = CreateTrayController();
  if (!wallpaperHost_ || !decodePipeline_ || !trayController_) {
    return false;
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
  if (!metricsLogFile_.EnsureReady()) {
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
    WaitMainLoopInterval(sleepMs, useMessageAwareWait);
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

void App::SyncTrayMenuState() const {
  if (!trayController_) {
    return;
  }
  TrayMenuState state;
  state.fpsCap = NormalizeFpsCap(config_.fpsCap);
  state.autoStart = config_.autoStart;
  state.adaptiveQuality = config_.adaptiveQuality;
  state.hasVideo = !config_.videoPath.empty();
  trayController_->UpdateMenuState(state);
}

void App::ScheduleConfigSave() {
  (void)configStore_.SaveExpected(config_);
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
    presentSamplesMs_.clear();
    lastPresentedAt_ = RenderScheduler::Clock::time_point{};
    return;
  }
  wallpaperHost_->DetachFromDesktop();
  wallpaperAttached_ = false;
  frame_bridge::ClearLatestFrame();
  presentSamplesMs_.clear();
  lastPresentedAt_ = RenderScheduler::Clock::time_point{};
}

void App::ResetPlaybackState(const bool resetLongRunState) {
  hasLastPresentedFrame_ = false;
  lastPresentedAt_ = RenderScheduler::Clock::time_point{};
  syntheticSequence_ = 0;
  lastDecodedTimestamp100ns_ = -1;
  ResetSourceFrameRateState(&sourceFrameRateState_);
  trayMenuVisible_ = false;
  lastTrayInteractionAt_ = RenderScheduler::Clock::time_point{};
  lastDecodeMode_ = DecodeMode::kUnknown;
  lastDecodePath_ = DecodePath::kUnknown;
  decodeCopyBytesInWindow_ = 0;
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
  hardSuspendedByPause_ = false;
  pauseTransitionState_ = PauseTransitionState{};
  decodePumpDynamicBoostMs_.store(0);
  decodeThreadQos_.store(static_cast<int>(RuntimeThreadQos::kNormal));
  decodeWarmupActive_.store(false);
  decodeOpenLongRunLevel_ = 0;
  {
    std::lock_guard<std::mutex> lock(decodedTokenMu_);
    hasLatestDecodedToken_ = false;
    latestDecodedToken_ = FrameToken{};
  }
  latestDecodedSequence_.store(0, std::memory_order_release);
}

bool App::StartVideoPipelineForPath(const std::string& path, const int longRunLoadLevel,
                                    const bool resetLongRunState,
                                    const bool startDecodeImmediately,
                                    const bool allowCachedPathProbe) {
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
  openProfile.codecPolicy = config_.codecPolicy;
  openProfile.adaptiveQualityEnabled = config_.adaptiveQuality;
  openProfile.longRunLoadLevel = longRunLoadLevel;
  openProfile.preferHardwareTransforms = true;
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
  if (startDecodeImmediately) {
    WakeDecodePump();
  }
  ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
  scheduler_.Reset();
  return true;
}

void App::ApplyRenderFpsCap(const int governorFps) {
  int desired = NormalizeFpsCap(governorFps);
  if (sourceFrameRateState_.sourceFps <= 30 && desired > 30) {
    desired = 30;
  }
  int baseHotSleepMs = ComputeDecodePumpHotSleepMs(desired, sourceFrameRateState_.sourceFps);
  int dynamicBoostMs = decodePumpDynamicBoostMs_.load();
  if (IsCpuFallbackDecodePath(lastDecodePath_)) {
    // CPU-only 回退链路下进一步放缓解码拉帧频率，降低长期动态 CPU 压力。
    baseHotSleepMs += (desired >= 60 ? 10 : 14);
    if (dynamicBoostMs > 0) {
      dynamicBoostMs += 8;
    }
  }
  const int nextHotSleepMs = std::clamp(baseHotSleepMs + dynamicBoostMs, 6, 64);
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

bool App::ApplyVideoPath(const std::string& newPath) {
  if (!decodePipeline_) {
    return false;
  }
  if (IsSameVideoPath(newPath, config_.videoPath)) {
    return true;
  }

  const std::string oldPath = config_.videoPath;
  const bool canRestoreOldPath =
      ShouldActivateVideoPipelineCached(oldPath, false, RenderScheduler::Clock::now());

  const auto restoreOldVideo = [this, &oldPath, canRestoreOldPath]() {
    if (canRestoreOldPath) {
      StartVideoPipelineForPath(oldPath);
    }
  };

  if (newPath.empty()) {
    decodePipeline_->Stop();
    decodeOpened_.store(false);
    decodeRunning_.store(false);
    decodeFrameReadyNotifierAvailable_ = false;
    ResetPlaybackState();
    config_.videoPath.clear();
    InvalidateVideoPathProbeCache();
    DetachWallpaper();
    return true;
  }

  if (!ShouldActivateVideoPipelineCached(newPath, false, RenderScheduler::Clock::now())) {
    return false;
  }

  if (!StartVideoPipelineForPath(newPath)) {
    restoreOldVideo();
    return false;
  }

  config_.videoPath = newPath;
  return true;
}

void App::StartDecodePump() {
  if (decodePumpRunning_.exchange(true)) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(decodePumpWaitMu_);
    decodePumpWakeRequested_ = false;
  }

  decodePumpThread_ = std::thread([this]() {
    const auto sleepInterruptible = [this](const int sleepMs,
                                           const bool preferEventDrivenWait) {
      if (sleepMs <= 0) {
        return;
      }
      const int boundedSleepMs = std::clamp(sleepMs, 1, 500);
      // SourceReader 回调可唤醒时，优先放大等待窗口以减少无帧轮询唤醒。
      const int waitMs =
          preferEventDrivenWait ? std::max(boundedSleepMs, 140) : boundedSleepMs;
      std::unique_lock<std::mutex> lock(decodePumpWaitMu_);
      decodePumpWaitCv_.wait_for(lock, std::chrono::milliseconds(waitMs),
                                 [this]() { return !decodePumpRunning_.load() || decodePumpWakeRequested_; });
      decodePumpWakeRequested_ = false;
    };

#ifdef _WIN32
    int lastDecodeThreadPriority = THREAD_PRIORITY_NORMAL;
    RuntimeThreadQos lastDecodeThreadQos = RuntimeThreadQos::kNormal;
    ULONG lastMemoryPriority = MEMORY_PRIORITY_NORMAL;
    const auto setDecodeThreadPriority = [&](const int priority) {
      if (priority == lastDecodeThreadPriority) {
        return;
      }
      if (SetThreadPriority(GetCurrentThread(), priority) != FALSE) {
        lastDecodeThreadPriority = priority;
      }
    };
    const auto applyDecodeThreadServiceHints = [&](const bool decodeReady,
                                                   const bool warmupActive) {
      const RuntimeThreadQos nextQos =
          warmupActive ? RuntimeThreadQos::kNormal : RuntimeThreadQos::kEco;
      const ULONG nextMemoryPriority =
          warmupActive ? MEMORY_PRIORITY_NORMAL : MEMORY_PRIORITY_LOW;
      decodeThreadQos_.store(static_cast<int>(nextQos));
      if (nextQos != lastDecodeThreadQos) {
        THREAD_POWER_THROTTLING_STATE throttling{};
        throttling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        throttling.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
        throttling.StateMask =
            nextQos == RuntimeThreadQos::kEco ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0;
        SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &throttling,
                             sizeof(throttling));
        lastDecodeThreadQos = nextQos;
      }
      if (nextMemoryPriority != lastMemoryPriority) {
        MEMORY_PRIORITY_INFORMATION memoryInfo{};
        memoryInfo.MemoryPriority = nextMemoryPriority;
        SetThreadInformation(GetCurrentThread(), ThreadMemoryPriority, &memoryInfo,
                             sizeof(memoryInfo));
        lastMemoryPriority = nextMemoryPriority;
      }
      setDecodeThreadPriority(decodeReady ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_IDLE);
    };
    applyDecodeThreadServiceHints(false, false);
#endif

    int decodeIdleSleepMs = 2;
    while (decodePumpRunning_.load()) {
      const bool decodeReady = decodePipeline_ && decodeOpened_.load() && decodeRunning_.load();
      const bool warmupActive = decodeWarmupActive_.load();
#ifdef _WIN32
      applyDecodeThreadServiceHints(decodeReady, warmupActive);
#else
      decodeThreadQos_.store(static_cast<int>(warmupActive ? RuntimeThreadQos::kNormal
                                                           : RuntimeThreadQos::kEco));
#endif
      if (!decodeReady) {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(false, false, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  false));
        continue;
      }
      if (decodeIdleSleepMs > 2) {
        decodeIdleSleepMs = 2;
      }

      FrameToken token{};
      if (decodePipeline_->TryAcquireLatestFrame(&token)) {
        std::lock_guard<std::mutex> lock(decodedTokenMu_);
        latestDecodedToken_ = token;
        hasLatestDecodedToken_ = true;
        latestDecodedSequence_.store(token.sequence, std::memory_order_release);
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, true, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        const int hotSleepMs = decodePumpHotSleepMs_.load();
        if (hotSleepMs > decodeIdleSleepMs) {
          decodeIdleSleepMs = hotSleepMs;
        }
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  true));
      } else {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, false, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  false));
      }
    }
  });
}

void App::StopDecodePump() {
  if (!decodePumpRunning_.exchange(false)) {
    return;
  }
  WakeDecodePump();
  if (decodePumpThread_.joinable()) {
    decodePumpThread_.join();
  }
}

void App::WakeDecodePump() {
  bool shouldNotify = false;
  {
    std::lock_guard<std::mutex> lock(decodePumpWaitMu_);
    shouldNotify = ShouldNotifyDecodePumpWake(decodePumpWakeRequested_);
    if (shouldNotify) {
      decodePumpWakeRequested_ = true;
    }
  }
  if (!shouldNotify) {
    return;
  }
  decodePumpWaitCv_.notify_one();
}

void App::OnDecodeFrameReady() { WakeDecodePump(); }

void App::OnDecodeFrameReadyThunk(void* const context) {
  if (context == nullptr) {
    return;
  }
  static_cast<App*>(context)->OnDecodeFrameReady();
}

bool App::HandleTrayActions() {
  if (!trayController_) {
    return true;
  }

  bool configChanged = false;
  bool trayStateChanged = false;
  bool hadTrayInteraction = false;
  TrayAction action;
  while (trayController_->TryDequeueAction(&action)) {
    switch (action.type) {
      case TrayActionType::kExit:
        hadTrayInteraction = true;
        RequestStop();
        return false;
      case TrayActionType::kSetFps30:
        hadTrayInteraction = true;
        if (config_.fpsCap != 30) {
          config_.fpsCap = 30;
          qualityGovernor_.SetTargetFps(config_.fpsCap);
          ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kSetFps60:
        hadTrayInteraction = true;
        if (config_.fpsCap != 60) {
          config_.fpsCap = 60;
          qualityGovernor_.SetTargetFps(config_.fpsCap);
          ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kSelectVideo:
        hadTrayInteraction = true;
        if (!action.payload.empty() && ApplyVideoPath(action.payload)) {
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kClearVideo:
        hadTrayInteraction = true;
        if (ApplyVideoPath({})) {
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kEnableAutoStart:
        hadTrayInteraction = true;
        if (!config_.autoStart && SetAutoStartEnabled(true)) {
          config_.autoStart = true;
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kDisableAutoStart:
        hadTrayInteraction = true;
        if (config_.autoStart && SetAutoStartEnabled(false)) {
          config_.autoStart = false;
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kEnableAdaptiveQuality:
        hadTrayInteraction = true;
        if (!config_.adaptiveQuality) {
          config_.adaptiveQuality = true;
          qualityGovernor_.SetEnabled(true);
          ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
          if (ShouldActivateVideoPipelineCached(config_.videoPath, false,
                                                RenderScheduler::Clock::now()) &&
              decodePipeline_ &&
              decodeOpened_.load()) {
            // 自适应质量切换后重开解码管线，让 MF 输出尺寸策略立即生效。
            StartVideoPipelineForPath(config_.videoPath);
          }
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kDisableAdaptiveQuality:
        hadTrayInteraction = true;
        if (config_.adaptiveQuality) {
          config_.adaptiveQuality = false;
          qualityGovernor_.SetEnabled(false);
          ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
          if (ShouldActivateVideoPipelineCached(config_.videoPath, false,
                                                RenderScheduler::Clock::now()) &&
              decodePipeline_ &&
              decodeOpened_.load()) {
            // 关闭后恢复全质量输出，保持行为可预期。
            StartVideoPipelineForPath(config_.videoPath);
          }
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kMenuOpened:
        trayMenuVisible_ = true;
        break;
      case TrayActionType::kMenuClosed:
        trayMenuVisible_ = false;
        hadTrayInteraction = true;
        break;
      case TrayActionType::kNone:
      default:
        break;
    }
  }

  if (hadTrayInteraction) {
    // 托盘菜单/文件对话框交互后短窗口内冻结上下文探测，避免桌面状态抖动误判。
    lastTrayInteractionAt_ = RenderScheduler::Clock::now();
  }

  if (configChanged) {
    ScheduleConfigSave();
  }
  if (trayStateChanged) {
    SyncTrayMenuState();
  }
  return true;
}

void App::Tick() {
  if (!decodePipeline_ || !wallpaperHost_ || !wallpaperAttached_) {
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
      presentSamplesMs_.clear();
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
            decodePipeline_->Open(config_.videoPath, DecodeOpenProfile{config_.codecPolicy,
                                                                       config_.adaptiveQuality,
                                                                       decodeOpenLongRunLevel_,
                                                                       true, false})) {
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
    if (StartVideoPipelineForPath(config_.videoPath, decodeOpenLongRunLevel_, false, true, true)) {
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
    lastDecodeMode_ = frame.decodeMode;
    lastDecodePath_ = frame.decodePath;
    decodeCopyBytesInWindow_ += frame.cpuCopyBytes;
    if (frame.width > 0 && frame.height > 0) {
      lastDecodeOutputPixels_ =
          static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    }
    if (frame.decodeMode == DecodeMode::kMediaFoundation) {
      const int observedSourceFps =
          UpdateSourceFrameRateState(lastDecodedTimestamp100ns_, frame.timestamp100ns,
                                     &sourceFrameRateState_);
      (void)observedSourceFps;
      lastDecodedTimestamp100ns_ = frame.timestamp100ns;
    } else {
      ResetSourceFrameRateState(&sourceFrameRateState_);
      lastDecodedTimestamp100ns_ = -1;
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

  constexpr std::chrono::milliseconds kStaleFrameKeepAliveInterval(250);
  const bool staleFramePresentDue =
      ShouldPresentStaleFrame(now, lastPresentedAt_, kStaleFrameKeepAliveInterval);
  if (!ShouldPresentFrame(hasNewDecodedToken, hasLastPresentedFrame_, staleFramePresentDue)) {
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  const auto presentBegin = RenderScheduler::Clock::now();
  wallpaperHost_->Present(frame);
  const auto presentEnd = RenderScheduler::Clock::now();
  lastPresentedAt_ = presentEnd;
  const double presentMs =
      std::chrono::duration<double, std::milli>(presentEnd - presentBegin).count();
  MaybeSampleAndLogMetrics(true, false, presentMs);
}

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
      SelectMetricsSampleInterval(hasActiveVideo, stablePauseForLoopSleep_, occluded);
  if (lastMetricsAt_ != RenderScheduler::Clock::time_point{} &&
      now - lastMetricsAt_ < metricsSampleInterval) {
    return;
  }
  lastMetricsAt_ = now;

  RuntimeMetrics metrics;
  metrics.decodeOutputPixels = lastDecodeOutputPixels_;
  metrics.threadQos =
      static_cast<RuntimeThreadQos>(decodeThreadQos_.load());
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
      ShouldExecuteLongRunDecodeTrim(longRunDecision.requestDecodeTrim, decodeRunning_.load(),
                                     lastDecodePath_)) {
    decodePipeline_->TrimMemory();
  }
  if (ShouldRequestWorkingSetTrim(hasActiveVideo, lastDecodePath_, metrics.workingSetBytes,
                                  longRunLoadState_.level)) {
    constexpr std::chrono::seconds kWorkingSetTrimInterval(2);
    if (lastWorkingSetTrimAt_ == RenderScheduler::Clock::time_point{} ||
        (now - lastWorkingSetTrimAt_) >= kWorkingSetTrimInterval) {
      TrimCurrentProcessWorkingSet();
      lastWorkingSetTrimAt_ = now;
    }
  }
  if (hasActiveVideo && config_.adaptiveQuality &&
      IsCpuFallbackDecodePath(lastDecodePath_)) {
    const int desiredDecodeOpenLevel = longRunLoadState_.level >= 2 ? 2 : 0;
    if (desiredDecodeOpenLevel != decodeOpenLongRunLevel_) {
      const std::string currentPath = config_.videoPath;
      if (ShouldActivateVideoPipelineCached(currentPath, false, now)) {
        decodePipeline_->Stop();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        StartVideoPipelineForPath(currentPath, desiredDecodeOpenLevel, false);
      }
    }
  }
  ApplyRenderFpsCap(effectiveFps);
  const int appliedFps = scheduler_.GetFpsCap();
  const int targetFps = NormalizeFpsCap(config_.fpsCap);

  if (!metricsLogFile_.Append(BuildMetricsCsvLine(NowUnixMs(), metrics, metricsSessionId_,
                                                  targetFps, appliedFps,
                                                  config_.adaptiveQuality, lastDecodeMode_,
                                                  lastDecodePath_, longRunLoadState_.level,
                                                  decodePumpHotSleepMs_.load(),
                                                  decodeCopyBytesInWindow_))) {
    // I/O 失败时静默降级，避免主渲染循环被监控路径反向影响。
  }

  totalFrames_ = 0;
  droppedFrames_ = 0;
  decodeCopyBytesInWindow_ = 0;
}

}  // namespace wallpaper
