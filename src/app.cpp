#include "wallpaper/app.h"

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wallpaper {
namespace {

RuntimeMetrics BuildMinimalMetrics() {
  RuntimeMetrics metrics;
  metrics.cpuPercent = 0.0;
  metrics.privateWorkingSetBytes = 0;
  metrics.presentP95Ms = 0.0;
  metrics.droppedFrameRatio = 0.0;
  return metrics;
}

#ifdef _WIN32
bool IsSessionInteractive() {
  // 锁屏/切换用户时通常无法获取输入桌面，借此快速判定会话不可交互。
  HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
  if (desktop == nullptr) {
    return false;
  }
  CloseDesktop(desktop);
  return true;
}

ForegroundState DetectForegroundState() {
  const HWND hwnd = GetForegroundWindow();
  if (hwnd == nullptr) {
    return ForegroundState::kUnknown;
  }

  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(hwnd, &placement)) {
    return ForegroundState::kUnknown;
  }
  if (placement.showCmd == SW_SHOWMAXIMIZED) {
    return ForegroundState::kFullscreen;
  }

  RECT windowRect{};
  if (!GetWindowRect(hwnd, &windowRect)) {
    return ForegroundState::kUnknown;
  }
  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (monitor == nullptr) {
    return ForegroundState::kWindowed;
  }
  MONITORINFO monitorInfo{};
  monitorInfo.cbSize = sizeof(monitorInfo);
  if (!GetMonitorInfoW(monitor, &monitorInfo)) {
    return ForegroundState::kWindowed;
  }

  const RECT& monitorRect = monitorInfo.rcMonitor;
  const bool coversMonitor = windowRect.left <= monitorRect.left &&
                             windowRect.top <= monitorRect.top &&
                             windowRect.right >= monitorRect.right &&
                             windowRect.bottom >= monitorRect.bottom;
  return coversMonitor ? ForegroundState::kFullscreen : ForegroundState::kWindowed;
}
#else
bool IsSessionInteractive() { return true; }
ForegroundState DetectForegroundState() { return ForegroundState::kWindowed; }
#endif

}  // namespace

App::App(std::filesystem::path configPath)
    : configStore_(std::move(configPath)), scheduler_(30), metrics_(60) {}

App::~App() { RequestStop(); }

bool App::Initialize() {
  config_ = configStore_.LoadAsync().get();
  scheduler_.SetFpsCap(config_.fpsCap);
  arbiter_.SetPauseOnFullscreen(config_.pauseOnFullscreen);

  wallpaperHost_ = CreateWallpaperHost();
  decodePipeline_ = CreateDecodePipeline();
  trayController_ = CreateTrayController();
  if (!wallpaperHost_ || !decodePipeline_ || !trayController_) {
    return false;
  }

  if (!wallpaperHost_->AttachToDesktop()) {
    return false;
  }
  wallpaperHost_->ResizeForDisplays();

  // 即使没有配置视频，也启动内置动态源模式，保证开箱即有动态效果。
  if (!decodePipeline_->Open(config_.videoPath, config_.codecPolicy)) {
    return false;
  }
  decodeOpened_ = true;
  if (!decodePipeline_->Start()) {
    return false;
  }
  decodeRunning_ = true;

  return true;
}

int App::Run() {
  running_.store(true);
  trayController_->StartMessageLoop();

  while (running_.load()) {
    if (trayController_->IsExitRequested()) {
      RequestStop();
      break;
    }
    Tick();

    // 按当前仲裁状态调整睡眠，避免 pause 状态下 1ms 忙轮询造成无效 CPU 占用。
    if (arbiter_.ShouldPause()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  if (decodePipeline_) {
    decodePipeline_->Stop();
    decodeRunning_ = false;
  }
  if (wallpaperHost_) {
    wallpaperHost_->DetachFromDesktop();
  }
  if (trayController_) {
    trayController_->StopMessageLoop();
  }
  return 0;
}

void App::RequestStop() { running_.store(false); }

void App::Tick() {
  if (!decodePipeline_ || !wallpaperHost_) {
    return;
  }

  arbiter_.SetSessionActive(IsSessionInteractive());
  arbiter_.SetDesktopVisible(true);
  static bool wasPaused = false;
  arbiter_.SetForegroundState(DetectForegroundState());
  const bool shouldPause = arbiter_.ShouldPause();

  if (shouldPause) {
    // 前台全屏或会话不可见时立即停解码并重置调度，确保资源优先让给前台应用。
    if (decodeRunning_) {
      decodePipeline_->Pause();
      decodeRunning_ = false;
    }
    scheduler_.Reset();
    wasPaused = true;
    return;
  }

  if (wasPaused) {
    // 从 pause 恢复后重置调度，让 ShouldRender() 立即放行首帧，减少恢复黑屏/静止时间。
    scheduler_.Reset();
    wasPaused = false;
  }

  if (decodeOpened_ && !decodeRunning_) {
    decodeRunning_ = decodePipeline_->Start();
  }

  if (!scheduler_.ShouldRender(RenderScheduler::Clock::now())) {
    return;
  }

  FrameToken frame{};
  if (!decodePipeline_->TryAcquireLatestFrame(&frame)) {
    RuntimeMetrics metrics = BuildMinimalMetrics();
    metrics.droppedFrameRatio = 1.0;
    metrics_.PushSample(metrics);
    return;
  }

  wallpaperHost_->Present(frame);
  RuntimeMetrics metrics = BuildMinimalMetrics();
  metrics.droppedFrameRatio = 0.0;
  metrics_.PushSample(metrics);
}

}  // namespace wallpaper
