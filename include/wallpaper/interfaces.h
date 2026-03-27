#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "wallpaper/config.h"

namespace wallpaper {

struct FrameToken final {
  std::uint64_t sequence = 0;
  std::int64_t timestamp100ns = 0;
};

class IWallpaperHost {
 public:
  virtual ~IWallpaperHost() = default;

  virtual bool AttachToDesktop() = 0;
  virtual void DetachFromDesktop() = 0;
  virtual void ResizeForDisplays() = 0;
  virtual void Present(const FrameToken& frame) = 0;
};

class IDecodePipeline {
 public:
  virtual ~IDecodePipeline() = default;

  virtual bool Open(const std::string& path, CodecPolicy codecPolicy) = 0;
  virtual bool Start() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual bool TryAcquireLatestFrame(FrameToken* frame) = 0;
};

class ITrayController {
 public:
  virtual ~ITrayController() = default;

  virtual void StartMessageLoop() = 0;
  virtual void StopMessageLoop() = 0;
  [[nodiscard]] virtual bool IsExitRequested() const = 0;
};

std::unique_ptr<IWallpaperHost> CreateWallpaperHost();
std::unique_ptr<IDecodePipeline> CreateDecodePipeline();
std::unique_ptr<ITrayController> CreateTrayController();

}  // namespace wallpaper
