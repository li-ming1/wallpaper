#pragma once

#include <atomic>
#include <filesystem>
#include <future>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "wallpaper/config_store.h"
#include "wallpaper/interfaces.h"
#include "wallpaper/metrics_log_file.h"
#include "wallpaper/metrics_sampler.h"
#include "wallpaper/quality_governor.h"
#include "wallpaper/pause_transition_policy.h"
#include "wallpaper/pause_suspend_policy.h"
#include "wallpaper/render_scheduler.h"
#include "wallpaper/resource_arbiter.h"

namespace wallpaper {

class App final {
 public:
  explicit App(std::filesystem::path configPath);
  ~App();

  bool Initialize();
  int Run();
  void RequestStop();

 private:
  bool HandleTrayActions();
  bool ApplyVideoPath(const std::string& newPath);
  bool EnsureWallpaperAttached();
  void DetachWallpaper();
  void ResetPlaybackState();
  bool StartVideoPipelineForPath(const std::string& path);
  void ApplyRenderFpsCap(int governorFps);
  void StartDecodePump();
  void StopDecodePump();
  void SyncTrayMenuState() const;
  void ScheduleConfigSave();
  void Tick();
  void MaybeSampleAndLogMetrics(bool attemptedRender, bool frameDropped, double presentMs);

  ConfigStore configStore_;
  Config config_{};
  RenderScheduler scheduler_;
  ResourceArbiter arbiter_;
  MetricsLogFile metricsLogFile_;
  MetricsSampler metrics_;
  QualityGovernor qualityGovernor_;
  std::string metricsSessionId_;

  std::unique_ptr<IWallpaperHost> wallpaperHost_;
  std::unique_ptr<IDecodePipeline> decodePipeline_;
  std::unique_ptr<ITrayController> trayController_;
  std::thread decodePumpThread_;
  std::atomic<bool> decodePumpRunning_{false};
  std::atomic<int> decodePumpHotSleepMs_{4};
  std::mutex decodedTokenMu_;
  FrameToken latestDecodedToken_{};
  bool hasLatestDecodedToken_ = false;

  std::atomic<bool> running_{false};
  std::atomic<bool> decodeOpened_{false};
  std::atomic<bool> decodeRunning_{false};
  bool wallpaperAttached_ = false;
  std::future<void> pendingSave_;
  RenderScheduler::Clock::time_point lastMetricsAt_{};
  std::vector<double> presentSamplesMs_;
  DecodeMode lastDecodeMode_ = DecodeMode::kUnknown;
  FrameToken lastPresentedFrame_{};
  bool hasLastPresentedFrame_ = false;
  std::uint64_t syntheticSequence_ = 0;
  std::int64_t lastDecodedTimestamp100ns_ = -1;
  int sourceFpsCap_ = 60;
  int sourceFpsHint30_ = 0;
  int sourceFpsHint60_ = 0;
  RenderScheduler::Clock::time_point lastSessionProbeAt_{};
  RenderScheduler::Clock::time_point lastForegroundProbeAt_{};
  bool cachedSessionInteractive_ = true;
  bool cachedDesktopContextActive_ = true;
  bool stablePauseForLoopSleep_ = false;
  bool wasPaused_ = false;
  bool decodeCacheTrimmedByPause_ = false;
  bool resourcesReleasedByPause_ = false;
  bool resumePipelinePending_ = false;
  RenderScheduler::Clock::time_point nextResumeAttemptAt_{};
  bool resumeWarmupOpened_ = false;
  bool resumeWarmupStarted_ = false;
  RenderScheduler::Clock::time_point nextWarmupAttemptAt_{};
  RenderScheduler::Clock::time_point pauseEnteredAt_{};
  bool hardSuspendedByPause_ = false;
  PauseTransitionState pauseTransitionState_{};
  std::size_t droppedFrames_ = 0;
  std::size_t totalFrames_ = 0;
};

}  // namespace wallpaper
