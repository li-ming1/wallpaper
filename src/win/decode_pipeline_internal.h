#pragma once

#include "wallpaper/interfaces.h"
#include "wallpaper/cpu_frame_downscale.h"
#include "wallpaper/decode_async_read_policy.h"
#include "wallpaper/decode_output_policy.h"
#include "wallpaper/d3d11_interop_device.h"
#include "wallpaper/nv12_layout_policy.h"
#include "wallpaper/upload_texture_policy.h"
#include "wallpaper/upload_scale_policy.h"
#include "wallpaper/frame_bridge.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <new>

#ifdef _WIN32
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#endif

namespace wallpaper {

#ifdef _WIN32
class DecodePipelineStub;

class AsyncSourceReaderCallback final : public IMFSourceReaderCallback {
 public:
  explicit AsyncSourceReaderCallback(DecodePipelineStub* owner);

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags,
                            LONGLONG timestamp100ns, IMFSample* sample) override;
  STDMETHODIMP OnFlush(DWORD) override { return S_OK; }
  STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }

 void Detach();

 private:
  ~AsyncSourceReaderCallback() = default;

  volatile long refCount_ = 1;
  std::atomic<DecodePipelineStub*> owner_{nullptr};
};
#endif  // _WIN32

class DecodePipelineStub final : public IDecodePipeline {
 public:
  ~DecodePipelineStub() override;

  bool Open(const std::string& path, const DecodeOpenProfile& profile) override;
  bool Start() override;
  void Pause() override;
  void Stop() override;
  void TrimMemory() override;
  bool TryAcquireLatestFrame(FrameToken* frame) override;
  void SetFrameReadyNotifier(const DecodeFrameReadyNotifyFn notifyFn,
                             void* const context) override;
  [[nodiscard]] bool SupportsFrameReadyNotifier() const override;

 private:
  friend class AsyncSourceReaderCallback;

  using Clock = std::chrono::steady_clock;

  enum class Mode {
    kFallbackTicker = 0,
    kMediaFoundation = 1,
  };

#ifdef _WIN32
  struct ReadySampleSnapshot final {
    IMFSample* sample = nullptr;
    std::int64_t rawTimestamp100ns = 0;
    std::int64_t outputTimestamp100ns = 0;
    std::uint64_t sequence = 0;
    std::uint32_t frameWidth = 0;
    std::uint32_t frameHeight = 0;
    std::uint32_t frameStride = 0;
    bool mfGpuZeroCopyActive = false;
    GUID selectedOutputSubtype = GUID{};
  };

  struct PublishResult final {
    bool ok = false;
    bool gpuBacked = false;
    std::size_t cpuCopyBytes = 0;
    std::uint32_t outputWidth = 0;
    std::uint32_t outputHeight = 0;
  };
#endif

  bool TryAcquireFallbackFrameLocked(FrameToken* frame);
  void ResetStateLocked();

#ifdef _WIN32
  static void QueryDesktopFrameHint(UINT32* outWidth, UINT32* outHeight);
  bool TryOpenMediaFoundationLocked(const std::string& path);
  bool EnsureMfStartupLocked();
  void ReleaseMfLocked();
  bool EnsureSharedNv12BridgeTexturesLocked(ID3D11Device* device, UINT width, UINT height);
  void ReleaseSharedNv12BridgeTexturesLocked();
  bool SeekReaderToStartLocked();
  void ClearAsyncReadySampleLocked();
  bool IssueAsyncReadLocked();
  void PumpDecodeAsyncReadsLocked();
  void HandleAsyncReadSample(HRESULT status, DWORD streamFlags, LONGLONG timestamp100ns,
                             IMFSample* sample);
  [[nodiscard]] static bool IsSelectedOutputNv12(const GUID& selectedOutputSubtype);
  [[nodiscard]] static DecodePath DecodePathForSelectedSubtype(const GUID& selectedOutputSubtype);
  bool TryTakeReadySampleSnapshotLocked(ReadySampleSnapshot* snapshot);
  PublishResult PublishSampleToBridgeUnlocked(IMFSample* sample,
                                              const ReadySampleSnapshot& snapshot);
  bool TryAcquireMediaFoundationFrame(FrameToken* frame);
#endif

  mutable std::mutex mu_;
  bool opened_ = false;
  bool running_ = false;
  std::string path_;
  Mode mode_ = Mode::kFallbackTicker;
  Clock::time_point timelineStart_{};
  Clock::time_point pauseAt_{};
  std::uint64_t sequence_ = 0;
  DecodeOpenProfile openProfile_{};
  DecodeFrameReadyNotifyFn frameReadyNotifyFn_ = nullptr;
  void* frameReadyNotifyContext_ = nullptr;

#ifdef _WIN32
  IMFSourceReader* sourceReader_ = nullptr;
  AsyncSourceReaderCallback* sourceReaderCallback_ = nullptr;
  IMFDXGIDeviceManager* dxgiDeviceManager_ = nullptr;
  UINT dxgiDeviceResetToken_ = 0;
  bool mfStarted_ = false;
  bool mfD3DInteropEnabled_ = false;
  bool mfGpuZeroCopyActive_ = false;
  DecodeInteropStage interopStage_ = DecodeInteropStage::kNotAttempted;
  std::int32_t interopHresult_ = 0;
  GUID selectedOutputSubtype_ = GUID{};
  std::uint32_t frameWidth_ = 0;
  std::uint32_t frameHeight_ = 0;
  std::uint32_t frameStride_ = 0;
  std::int64_t mfBaseOffset100ns_ = 0;
  std::int64_t mfLastRawTimestamp100ns_ = -1;
  std::int64_t mfLastOutputTimestamp100ns_ = -1;
  DecodeAsyncReadState decodeAsyncReadState_{};
  IMFSample* asyncReadySample_ = nullptr;
  std::int64_t asyncReadyRawTimestamp100ns_ = 0;
  ID3D11Texture2D* sharedNv12BridgeYTexture_ = nullptr;
  ID3D11Texture2D* sharedNv12BridgeUvTexture_ = nullptr;
  UINT sharedNv12BridgeWidth_ = 0;
  UINT sharedNv12BridgeHeight_ = 0;
#endif
};

std::unique_ptr<IDecodePipeline> CreateDecodePipeline();

}  // namespace wallpaper
