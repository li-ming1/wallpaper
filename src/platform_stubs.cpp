#include "wallpaper/interfaces.h"

#ifndef _WIN32
namespace wallpaper {
namespace {

class WallpaperHostStub final : public IWallpaperHost {
 public:
  bool AttachToDesktop() override { return true; }
  void DetachFromDesktop() override {}
  void ResizeForDisplays() override {}
  void Present(const FrameToken&) override {}
};

class DecodePipelineStub final : public IDecodePipeline {
 public:
  bool Open(const std::string&, CodecPolicy) override { return true; }
  bool Start() override { return true; }
  void Pause() override {}
  void Stop() override {}
  void TrimMemory() override {}
  bool TryAcquireLatestFrame(FrameToken*) override { return false; }
};

class TrayControllerStub final : public ITrayController {
 public:
  void StartMessageLoop() override {}
  void StopMessageLoop() override {}
  void UpdateMenuState(const TrayMenuState&) override {}
  [[nodiscard]] bool IsExitRequested() const override { return false; }
  bool TryDequeueAction(TrayAction*) override { return false; }
};

}  // namespace

std::unique_ptr<IWallpaperHost> CreateWallpaperHost() {
  return std::make_unique<WallpaperHostStub>();
}

std::unique_ptr<IDecodePipeline> CreateDecodePipeline() {
  return std::make_unique<DecodePipelineStub>();
}

std::unique_ptr<ITrayController> CreateTrayController() {
  return std::make_unique<TrayControllerStub>();
}

}  // namespace wallpaper
#endif
