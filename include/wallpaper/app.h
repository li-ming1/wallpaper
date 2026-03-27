#pragma once

#include <atomic>
#include <filesystem>
#include <memory>

#include "wallpaper/config_store.h"
#include "wallpaper/interfaces.h"
#include "wallpaper/metrics_sampler.h"
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
  void Tick();

  ConfigStore configStore_;
  Config config_{};
  RenderScheduler scheduler_;
  ResourceArbiter arbiter_;
  MetricsSampler metrics_;

  std::unique_ptr<IWallpaperHost> wallpaperHost_;
  std::unique_ptr<IDecodePipeline> decodePipeline_;
  std::unique_ptr<ITrayController> trayController_;

  std::atomic<bool> running_{false};
  bool decodeOpened_ = false;
  bool decodeRunning_ = false;
};

}  // namespace wallpaper
