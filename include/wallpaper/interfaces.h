#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "wallpaper/config.h"

namespace wallpaper {

enum class DecodeMode {
  kUnknown = 0,
  kMediaFoundation = 1,
  kFallbackTicker = 2,
};

enum class DecodePath {
  kUnknown = 0,
  kDxvaZeroCopy = 1,
  kCpuRgb32Fallback = 2,
  kCpuNv12Fallback = 3,
  kFallbackTicker = 4,
};

enum class DecodeInteropStage {
  kUnknown = 0,
  kNotAttempted = 1,
  kSharedDeviceMissing = 2,
  kDeviceManagerCreateFailed = 3,
  kDeviceManagerResetFailed = 4,
  kD3DManagerBindFailed = 5,
  kEnabled = 6,
};

[[nodiscard]] inline bool IsCpuFallbackDecodePath(const DecodePath decodePath) noexcept {
  return decodePath == DecodePath::kCpuRgb32Fallback ||
         decodePath == DecodePath::kCpuNv12Fallback;
}

struct FrameToken final {
  std::uint64_t sequence = 0;
  std::int64_t timestamp100ns = 0;
  int width = 0;
  int height = 0;
  DecodeMode decodeMode = DecodeMode::kUnknown;
  DecodePath decodePath = DecodePath::kUnknown;
  DecodeInteropStage decodeInteropStage = DecodeInteropStage::kUnknown;
  std::int32_t decodeInteropHresult = 0;
  bool gpuBacked = false;
  std::size_t cpuCopyBytes = 0;
  bool hasColor = false;
  float colorR = 0.0f;
  float colorG = 0.0f;
  float colorB = 0.0f;
};

class IWallpaperHost {
 public:
  virtual ~IWallpaperHost() = default;

  virtual bool AttachToDesktop() = 0;
  virtual void DetachFromDesktop() = 0;
  virtual void ResizeForDisplays() = 0;
  virtual void Present(const FrameToken& frame) = 0;
  [[nodiscard]] virtual bool IsOccluded() const = 0;
};

struct DecodeOpenProfile final {
  CodecPolicy codecPolicy = CodecPolicy::kH264;
  bool adaptiveQualityEnabled = true;
  int longRunLoadLevel = 0;
  bool preferHardwareTransforms = true;
  bool requireHardwareTransforms = false;
};

using DecodeFrameReadyNotifyFn = void (*)(void* context);

class IDecodePipeline {
 public:
  virtual ~IDecodePipeline() = default;

  virtual bool Open(const std::string& path, const DecodeOpenProfile& profile) = 0;
  virtual bool Start() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual void TrimMemory() = 0;
  virtual bool TryAcquireLatestFrame(FrameToken* frame) = 0;
  virtual void SetFrameReadyNotifier(DecodeFrameReadyNotifyFn notifyFn, void* context) = 0;
  [[nodiscard]] virtual bool SupportsFrameReadyNotifier() const = 0;
};

enum class TrayActionType {
  kNone = 0,
  kExit = 1,
  kSetFps30 = 2,
  kSetFps60 = 3,
  kSelectVideo = 4,
  kClearVideo = 5,
  kEnableAutoStart = 6,
  kDisableAutoStart = 7,
  kEnableAdaptiveQuality = 8,
  kDisableAdaptiveQuality = 9,
  kMenuOpened = 10,
  kMenuClosed = 11,
};

struct TrayAction final {
  TrayActionType type = TrayActionType::kNone;
  std::string payload;
};

struct TrayMenuState final {
  int fpsCap = 30;
  bool autoStart = false;
  bool adaptiveQuality = true;
  bool hasVideo = false;
};

class ITrayController {
 public:
  virtual ~ITrayController() = default;

  virtual void StartMessageLoop() = 0;
  virtual void StopMessageLoop() = 0;
  virtual void UpdateMenuState(const TrayMenuState& state) = 0;
  [[nodiscard]] virtual bool IsExitRequested() const = 0;
  virtual bool TryDequeueAction(TrayAction* action) = 0;
};

std::unique_ptr<IWallpaperHost> CreateWallpaperHost();
std::unique_ptr<IDecodePipeline> CreateDecodePipeline();
std::unique_ptr<ITrayController> CreateTrayController();

}  // namespace wallpaper
