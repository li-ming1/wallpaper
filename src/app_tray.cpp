#include "wallpaper/app.h"
#include "wallpaper/video_path_matcher.h"
#include "app_autostart.h"

namespace wallpaper {

void App::SyncTrayMenuState() const {
  if (!trayController_) {
    return;
  }
  TrayMenuState state;
  state.autoStart = config_.autoStart;
  state.hasVideo = !config_.videoPath.empty();
  trayController_->UpdateMenuState(state);
}

void App::ScheduleConfigSave() { (void)configStore_.SaveExpected(config_); }

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
        if (!config_.autoStart && wallpaper::SetAutoStartEnabled(true)) {
          config_.autoStart = true;
          configChanged = true;
          trayStateChanged = true;
        }
        break;
      case TrayActionType::kDisableAutoStart:
        hadTrayInteraction = true;
        if (config_.autoStart && wallpaper::SetAutoStartEnabled(false)) {
          config_.autoStart = false;
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

}  // namespace wallpaper
