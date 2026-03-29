#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "wallpaper/config_store.h"
#include "wallpaper/interfaces.h"
#include "wallpaper/long_run_load_policy.h"
#include "wallpaper/metrics_log_file.h"
#include "wallpaper/metrics_sampler.h"
#include "wallpaper/quality_governor.h"
#include "wallpaper/pause_transition_policy.h"
#include "wallpaper/pause_suspend_policy.h"
#include "wallpaper/render_scheduler.h"
#include "wallpaper/resource_arbiter.h"
#include "wallpaper/source_frame_rate_policy.h"

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
  void ResetPlaybackState(bool resetLongRunState = true);
  bool StartVideoPipelineForPath(const std::string& path, int longRunLoadLevel = 0,
                                 bool resetLongRunState = true,
                                 bool startDecodeImmediately = true);
  void ApplyRenderFpsCap(int governorFps);
  void StartDecodePump();
  void StopDecodePump();
  void WakeDecodePump();
  void OnDecodeFrameReady();
  static void OnDecodeFrameReadyThunk(void* context);
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
  bool decodeFrameReadyNotifierAvailable_ = false;
  std::atomic<int> decodePumpHotSleepMs_{4};
  std::atomic<int> decodePumpDynamicBoostMs_{0};
  std::atomic<int> decodeThreadQos_{static_cast<int>(RuntimeThreadQos::kNormal)};
  std::atomic<bool> decodeWarmupActive_{false};
  mutable std::mutex decodePumpWaitMu_;
  std::condition_variable decodePumpWaitCv_;
  bool decodePumpWakeRequested_ = false;
  std::mutex decodedTokenMu_;
  FrameToken latestDecodedToken_{};
  bool hasLatestDecodedToken_ = false;
  std::atomic<std::uint64_t> latestDecodedSequence_{0};

  std::atomic<bool> running_{false};
  std::atomic<bool> decodeOpened_{false};
  std::atomic<bool> decodeRunning_{false};
  bool wallpaperAttached_ = false;
  RenderScheduler::Clock::time_point lastMetricsAt_{};
  std::vector<double> presentSamplesMs_;
  DecodeMode lastDecodeMode_ = DecodeMode::kUnknown;
  DecodePath lastDecodePath_ = DecodePath::kUnknown;
  FrameToken lastPresentedFrame_{};
  bool hasLastPresentedFrame_ = false;
  std::uint64_t syntheticSequence_ = 0;
  std::int64_t lastDecodedTimestamp100ns_ = -1;
  SourceFrameRateState sourceFrameRateState_{};
  bool trayMenuVisible_ = false;
  RenderScheduler::Clock::time_point lastTrayInteractionAt_{};
  RenderScheduler::Clock::time_point lastSessionProbeAt_{};
  RenderScheduler::Clock::time_point lastForegroundProbeAt_{};
  int foregroundProbeFailureStreak_ = 0;
  bool cachedSessionInteractive_ = true;
  bool cachedDesktopContextActive_ = true;
  bool cachedBatterySaverActive_ = false;
  bool cachedRemoteSessionActive_ = false;
  LongRunLoadState longRunLoadState_{};
  bool stablePauseForLoopSleep_ = false;
  bool wasPaused_ = false;
  bool decodeCacheTrimmedByPause_ = false;
  bool resourcesReleasedByPause_ = false;
  bool resumePipelinePending_ = false;
  RenderScheduler::Clock::time_point nextResumeAttemptAt_{};
  bool resumeWarmupOpened_ = false;
  bool resumeWarmupStarted_ = false;
  RenderScheduler::Clock::time_point nextWarmupAttemptAt_{};
  bool startupDecodeDeferred_ = false;
  RenderScheduler::Clock::time_point startupDecodeDeferredAt_{};
  RenderScheduler::Clock::time_point pauseEnteredAt_{};
  bool hardSuspendedByPause_ = false;
  PauseTransitionState pauseTransitionState_{};
  RenderScheduler::Clock::time_point lastWorkingSetTrimAt_{};
  std::size_t droppedFrames_ = 0;
  std::size_t totalFrames_ = 0;
  std::size_t decodeCopyBytesInWindow_ = 0;
  std::size_t lastDecodeOutputPixels_ = 0;
  int decodeOpenLongRunLevel_ = 0;
};

}  // namespace wallpaper
