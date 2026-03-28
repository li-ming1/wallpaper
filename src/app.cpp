#include "wallpaper/app.h"
#include "wallpaper/desktop_context_policy.h"
#include "wallpaper/frame_bridge.h"
#include "wallpaper/loop_sleep_policy.h"
#include "wallpaper/metrics_log_line.h"
#include "wallpaper/pause_transition_policy.h"
#include "wallpaper/probe_cadence_policy.h"
#include "wallpaper/startup_policy.h"
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

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (process == nullptr) {
    return false;
  }

  wchar_t processPath[1024] = {};
  DWORD length = static_cast<DWORD>(std::size(processPath));
  const bool ok = QueryFullProcessImageNameW(process, 0, processPath, &length) != FALSE;
  CloseHandle(process);
  if (!ok || length == 0) {
    return false;
  }

  *outProcessName = ExtractBaseName(std::wstring(processPath, length));
  return true;
}

bool TryDetectDesktopContextActive(bool* outActive) {
  if (outActive == nullptr) {
    return false;
  }
  const HWND hwnd = GetForegroundWindow();
  if (hwnd == nullptr) {
    return false;
  }

  wchar_t className[256] = {};
  if (GetClassNameW(hwnd, className, 255) == 0) {
    return false;
  }

  std::wstring processName;
  TryQueryForegroundProcessBaseName(hwnd, &processName);
  *outActive = ShouldTreatAsDesktopContext(std::wstring(className), processName);
  return true;
}

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

std::size_t QueryPrivateBytes() {
  PROCESS_MEMORY_COUNTERS counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), counters.cb)) {
    return 0;
  }
  return static_cast<std::size_t>(counters.WorkingSetSize);
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
  ScopedHighResolutionTimer() {
    if (timeBeginPeriod(1) == TIMERR_NOERROR) {
      enabled_ = true;
    }
  }

  ~ScopedHighResolutionTimer() {
    if (enabled_) {
      timeEndPeriod(1);
    }
  }

 private:
  bool enabled_ = false;
};
#else
double QueryProcessCpuPercent() { return 0.0; }
std::size_t QueryPrivateBytes() { return 0; }
bool IsSessionInteractive() { return true; }
bool TrimCurrentProcessWorkingSet() { return false; }
bool SetAutoStartEnabled(bool) { return true; }
bool TryDetectDesktopContextActive(bool* outActive) {
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
      metricsSessionId_(BuildMetricsSessionId()) {}

App::~App() {
  RequestStop();
  StopDecodePump();
  if (pendingSave_.valid()) {
    pendingSave_.wait();
  }
}

bool App::Initialize() {
  config_ = configStore_.LoadAsync().get();
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

  if (ShouldActivateVideoPipeline(config_.videoPath)) {
    if (!StartVideoPipelineForPath(config_.videoPath)) {
      // 路径存在但启动失败时降级到“仅托盘运行”，避免出现不可控遮盖层。
      config_.videoPath.clear();
      DetachWallpaper();
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
    const bool hasActiveVideo = wallpaperAttached_ && decodeOpened_.load();
    const auto untilNextRender = std::chrono::duration_cast<std::chrono::milliseconds>(
        scheduler_.TimeUntilNextRender(RenderScheduler::Clock::now()));
    const int sleepMs =
        ComputeMainLoopSleepMs(shouldPause, hasActiveVideo, untilNextRender);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
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
  if (pendingSave_.valid()) {
    pendingSave_.wait();
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
  if (pendingSave_.valid() &&
      pendingSave_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    return;
  }
  pendingSave_ = configStore_.SaveAsync(config_);
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
    presentSamplesMs_.shrink_to_fit();
    return;
  }
  wallpaperHost_->DetachFromDesktop();
  wallpaperAttached_ = false;
  frame_bridge::ClearLatestFrame();
  presentSamplesMs_.clear();
  presentSamplesMs_.shrink_to_fit();
}

void App::ResetPlaybackState() {
  hasLastPresentedFrame_ = false;
  syntheticSequence_ = 0;
  lastDecodedTimestamp100ns_ = -1;
  sourceFpsCap_ = 60;
  sourceFpsHint30_ = 0;
  sourceFpsHint60_ = 0;
  trayMenuVisible_ = false;
  lastTrayInteractionAt_ = RenderScheduler::Clock::time_point{};
  lastDecodeMode_ = DecodeMode::kUnknown;
  lastSessionProbeAt_ = RenderScheduler::Clock::time_point{};
  lastForegroundProbeAt_ = RenderScheduler::Clock::time_point{};
  cachedSessionInteractive_ = true;
  cachedDesktopContextActive_ = true;
  stablePauseForLoopSleep_ = false;
  wasPaused_ = false;
  decodeCacheTrimmedByPause_ = false;
  resourcesReleasedByPause_ = false;
  resumePipelinePending_ = false;
  nextResumeAttemptAt_ = RenderScheduler::Clock::time_point{};
  resumeWarmupOpened_ = false;
  resumeWarmupStarted_ = false;
  nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
  pauseEnteredAt_ = RenderScheduler::Clock::time_point{};
  hardSuspendedByPause_ = false;
  pauseTransitionState_ = PauseTransitionState{};
  {
    std::lock_guard<std::mutex> lock(decodedTokenMu_);
    hasLatestDecodedToken_ = false;
    latestDecodedToken_ = FrameToken{};
  }
}

bool App::StartVideoPipelineForPath(const std::string& path) {
  if (!decodePipeline_ || !ShouldActivateVideoPipeline(path)) {
    return false;
  }
  if (!EnsureWallpaperAttached()) {
    return false;
  }
  if (!decodePipeline_->Open(path, config_.codecPolicy)) {
    decodeOpened_.store(false);
    decodeRunning_.store(false);
    return false;
  }
  decodeOpened_.store(true);
  if (!decodePipeline_->Start()) {
    decodePipeline_->Stop();
    decodeOpened_.store(false);
    decodeRunning_.store(false);
    return false;
  }
  decodeRunning_.store(true);
  ResetPlaybackState();
  ApplyRenderFpsCap(qualityGovernor_.CurrentFps());
  scheduler_.Reset();
  return true;
}

void App::ApplyRenderFpsCap(const int governorFps) {
  int desired = NormalizeFpsCap(governorFps);
  if (sourceFpsCap_ == 30 && desired > 30) {
    desired = 30;
  }
  decodePumpHotSleepMs_.store(ComputeDecodePumpHotSleepMs(desired));
  WakeDecodePump();
  if (scheduler_.GetFpsCap() != desired) {
    scheduler_.SetFpsCap(desired);
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
  const bool canRestoreOldPath = ShouldActivateVideoPipeline(oldPath);

  const auto restoreOldVideo = [this, &oldPath, canRestoreOldPath]() {
    if (canRestoreOldPath) {
      StartVideoPipelineForPath(oldPath);
    }
  };

  if (newPath.empty()) {
    decodePipeline_->Stop();
    decodeOpened_.store(false);
    decodeRunning_.store(false);
    ResetPlaybackState();
    config_.videoPath.clear();
    DetachWallpaper();
    return true;
  }

  if (!ShouldActivateVideoPipeline(newPath)) {
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

  decodePumpThread_ = std::thread([this]() {
    const auto sleepInterruptible = [this](const int sleepMs) {
      if (sleepMs <= 0) {
        return;
      }
      std::unique_lock<std::mutex> lock(decodePumpWaitMu_);
      decodePumpWaitCv_.wait_for(lock, std::chrono::milliseconds(sleepMs),
                                 [this]() { return !decodePumpRunning_.load(); });
    };

    int decodeIdleSleepMs = 2;
    while (decodePumpRunning_.load()) {
      const bool decodeReady = decodePipeline_ && decodeOpened_.load() && decodeRunning_.load();
      if (!decodeReady) {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(false, false, decodeIdleSleepMs);
        sleepInterruptible(decodeIdleSleepMs);
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
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, true, decodeIdleSleepMs);
        const int hotSleepMs = decodePumpHotSleepMs_.load();
        if (hotSleepMs > decodeIdleSleepMs) {
          decodeIdleSleepMs = hotSleepMs;
        }
        sleepInterruptible(decodeIdleSleepMs);
      } else {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, false, decodeIdleSleepMs);
        sleepInterruptible(decodeIdleSleepMs);
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
  decodePumpWaitCv_.notify_all();
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
  constexpr std::chrono::milliseconds kTrayInteractionProbeFreeze(1200);
  const bool inTrayInteractionFreeze =
      lastTrayInteractionAt_ != RenderScheduler::Clock::time_point{} &&
      now >= lastTrayInteractionAt_ &&
      (now - lastTrayInteractionAt_) <= kTrayInteractionProbeFreeze;
  const bool suppressDesktopContextProbe = trayMenuVisible_ || inTrayInteractionFreeze;
  if (ShouldRefreshRuntimeProbe(now, lastSessionProbeAt_, probeIntervals.session)) {
    cachedSessionInteractive_ = IsSessionInteractive();
    lastSessionProbeAt_ = now;
  }
  const bool shouldProbeForeground =
      ShouldRefreshRuntimeProbe(now, lastForegroundProbeAt_, probeIntervals.foreground);
  if (shouldProbeForeground && !suppressDesktopContextProbe) {
    bool desktopContextActive = cachedDesktopContextActive_;
    if (TryDetectDesktopContextActive(&desktopContextActive)) {
      cachedDesktopContextActive_ = desktopContextActive;
    }
    lastForegroundProbeAt_ = now;
  }
  arbiter_.SetSessionActive(cachedSessionInteractive_);
  arbiter_.SetDesktopContextActive(cachedDesktopContextActive_);
  arbiter_.SetDesktopVisible(true);
  const bool rawShouldPause = arbiter_.ShouldPause();
  constexpr std::chrono::milliseconds kPauseEnterDelay(110);
  constexpr std::chrono::milliseconds kPauseExitDelay(180);
  constexpr std::chrono::milliseconds kPauseExitDelayWarmup(360);
  constexpr std::chrono::milliseconds kTrimDecodeCacheAfter(2500);
  constexpr std::chrono::milliseconds kHardSuspendAfterAggressive(8000);
  constexpr std::chrono::milliseconds kHardSuspendAfterConservative(12000);
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
      nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
      pauseEnteredAt_ = now;
      scheduler_.Reset();
      // 保留最后一帧并仅停止解码，让切换到“静态壁纸”时更自然。
      presentSamplesMs_.clear();
      wasPaused_ = true;
    } else if (!hardSuspendedByPause_ && decodeOpened_.load()) {
      const auto pausedDuration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - pauseEnteredAt_);
      if (!decodeCacheTrimmedByPause_ &&
          ShouldTrimDecodeCacheDuringPause(pausedDuration, kTrimDecodeCacheAfter)) {
        // 轻暂停持续一段时间后释放 CPU 侧帧缓存，降低内存驻留峰值。
        decodePipeline_->TrimMemory();
        decodeCacheTrimmedByPause_ = true;
      }
      const bool allowAggressiveSuspend = arbiter_.ShouldAllowHardSuspend();
      const auto hardSuspendThreshold =
          SelectHardSuspendThreshold(allowAggressiveSuspend, kHardSuspendAfterAggressive,
                                     kHardSuspendAfterConservative);
      if (ShouldHardSuspendDuringPause(pausedDuration, hardSuspendThreshold)) {
        // 长暂停再升级为硬挂起，兼顾资源回收与短切换流畅性。
        decodePipeline_->Stop();
        TrimCurrentProcessWorkingSet();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        hardSuspendedByPause_ = true;
        resumeWarmupOpened_ = false;
        resumeWarmupStarted_ = false;
        nextWarmupAttemptAt_ = now;
        resourcesReleasedByPause_ = true;
      }
    } else if (hardSuspendedByPause_) {
      const bool shouldWarmResume = ShouldWarmResumeDuringPause(rawShouldPause, hardSuspendedByPause_);
      if (shouldWarmResume && !resumeWarmupOpened_ && now >= nextWarmupAttemptAt_) {
        if (ShouldActivateVideoPipeline(config_.videoPath) &&
            decodePipeline_->Open(config_.videoPath, config_.codecPolicy)) {
          // 在退出 pause 迟滞窗口内预热 Open，恢复时只需 Start，降低解冻卡顿。
          decodeOpened_.store(true);
          decodeRunning_.store(false);
          resumeWarmupOpened_ = true;
          resumeWarmupStarted_ = false;
          nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
        } else {
          nextWarmupAttemptAt_ = now + std::chrono::milliseconds(500);
        }
      } else if (shouldWarmResume && resumeWarmupOpened_ && !resumeWarmupStarted_) {
        if (decodePipeline_->Start()) {
          decodeRunning_.store(true);
          resumeWarmupStarted_ = true;
        } else {
          decodePipeline_->Stop();
          decodeOpened_.store(false);
          decodeRunning_.store(false);
          resumeWarmupOpened_ = false;
          resumeWarmupStarted_ = false;
          nextWarmupAttemptAt_ = now + std::chrono::milliseconds(500);
        }
      } else if (!shouldWarmResume && resumeWarmupOpened_) {
        // 预热后又回到暂停态时回收预热资源，避免频繁切换导致内存反复抬升。
        decodePipeline_->Stop();
        decodeOpened_.store(false);
        decodeRunning_.store(false);
        resumeWarmupOpened_ = false;
        resumeWarmupStarted_ = false;
        nextWarmupAttemptAt_ = now + std::chrono::milliseconds(500);
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
          resumePipelinePending_ = ShouldActivateVideoPipeline(config_.videoPath);
          nextResumeAttemptAt_ = now;
        } else {
          decodeRunning_.store(true);
          resumeWarmupStarted_ = true;
        }
      } else {
        resumePipelinePending_ = ShouldActivateVideoPipeline(config_.videoPath);
        nextResumeAttemptAt_ = now;
      }
    } else if (decodeOpened_.load() && !decodeRunning_.load()) {
      decodeRunning_.store(decodePipeline_->Start());
    }
    resourcesReleasedByPause_ = false;
    hardSuspendedByPause_ = false;
    decodeCacheTrimmedByPause_ = false;
    resumeWarmupOpened_ = false;
    resumeWarmupStarted_ = false;
    nextWarmupAttemptAt_ = RenderScheduler::Clock::time_point{};
    pauseEnteredAt_ = RenderScheduler::Clock::time_point{};
    wasPaused_ = false;
  }

  if (resumePipelinePending_ && now >= nextResumeAttemptAt_) {
    if (ShouldActivateVideoPipeline(config_.videoPath) &&
        StartVideoPipelineForPath(config_.videoPath)) {
      resumePipelinePending_ = false;
      nextResumeAttemptAt_ = RenderScheduler::Clock::time_point{};
    } else {
      nextResumeAttemptAt_ = now + std::chrono::seconds(1);
    }
  }

  if (decodeOpened_.load() && !decodeRunning_.load()) {
    decodeRunning_.store(decodePipeline_->Start());
  }

  if (!scheduler_.ShouldRender(RenderScheduler::Clock::now())) {
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  FrameToken frame{};
  bool hasNewDecodedToken = false;
  {
    std::lock_guard<std::mutex> lock(decodedTokenMu_);
    if (hasLatestDecodedToken_ &&
        (!hasLastPresentedFrame_ ||
         latestDecodedToken_.sequence != lastPresentedFrame_.sequence)) {
      frame = latestDecodedToken_;
      hasNewDecodedToken = true;
    }
  }

  if (hasNewDecodedToken) {
    lastDecodeMode_ = frame.decodeMode;
    if (frame.decodeMode == DecodeMode::kMediaFoundation) {
      if (lastDecodedTimestamp100ns_ > 0 && frame.timestamp100ns > lastDecodedTimestamp100ns_) {
        const std::int64_t delta = frame.timestamp100ns - lastDecodedTimestamp100ns_;
        const bool hint30 = delta >= 300000 && delta <= 500000;
        const bool hint60 = delta >= 120000 && delta <= 220000;
        // 用时间戳窗口 + 累积计数做滞回，避免 30/60 之间抖动切换。
        if (hint30) {
          if (sourceFpsHint30_ < 8) {
            ++sourceFpsHint30_;
          }
          if (sourceFpsHint60_ > 0) {
            --sourceFpsHint60_;
          }
        } else if (hint60) {
          if (sourceFpsHint60_ < 8) {
            ++sourceFpsHint60_;
          }
          if (sourceFpsHint30_ > 0) {
            --sourceFpsHint30_;
          }
        }

        if (sourceFpsHint30_ >= 4) {
          sourceFpsCap_ = 30;
        } else if (sourceFpsHint60_ >= 4) {
          sourceFpsCap_ = 60;
        }
      }
      lastDecodedTimestamp100ns_ = frame.timestamp100ns;
    } else {
      sourceFpsCap_ = 60;
      lastDecodedTimestamp100ns_ = -1;
      sourceFpsHint30_ = 0;
      sourceFpsHint60_ = 0;
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

  if (!ShouldPresentFrame(hasNewDecodedToken, hasLastPresentedFrame_)) {
    MaybeSampleAndLogMetrics(false, false, 0.0);
    return;
  }

  const auto presentBegin = RenderScheduler::Clock::now();
  wallpaperHost_->Present(frame);
  const auto presentEnd = RenderScheduler::Clock::now();
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
  if (lastMetricsAt_ != RenderScheduler::Clock::time_point{} &&
      now - lastMetricsAt_ < std::chrono::seconds(1)) {
    return;
  }
  lastMetricsAt_ = now;

  RuntimeMetrics metrics;
  metrics.cpuPercent = QueryProcessCpuPercent();
  metrics.privateWorkingSetBytes = QueryPrivateBytes();
  metrics.presentP95Ms = TakeP95Ms(&presentSamplesMs_);
  metrics.droppedFrameRatio =
      totalFrames_ == 0 ? 0.0
                        : static_cast<double>(droppedFrames_) / static_cast<double>(totalFrames_);
  metrics_.PushSample(metrics);
  // 每秒基于实时负载做一次帧率档位决策，避免在每帧路径引入额外分支和抖动。
  const int effectiveFps = qualityGovernor_.Update(metrics);
  ApplyRenderFpsCap(effectiveFps);
  const int appliedFps = scheduler_.GetFpsCap();
  const int targetFps = NormalizeFpsCap(config_.fpsCap);

  if (!metricsLogFile_.Append(BuildMetricsCsvLine(NowUnixMs(), metrics, metricsSessionId_,
                                                  targetFps, appliedFps,
                                                  config_.adaptiveQuality, lastDecodeMode_))) {
    // I/O 失败时静默降级，避免主渲染循环被监控路径反向影响。
  }

  totalFrames_ = 0;
  droppedFrames_ = 0;
}

}  // namespace wallpaper
