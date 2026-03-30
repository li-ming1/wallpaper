#include "wallpaper/interfaces.h"
#include "wallpaper/decode_async_read_policy.h"
#include "wallpaper/decode_output_policy.h"
#include "wallpaper/decode_output_subtype_policy.h"
#include "wallpaper/d3d11_interop_device.h"
#include "wallpaper/nv12_layout_policy.h"
#include "wallpaper/source_frame_rate_policy.h"

#include "wallpaper/frame_bridge.h"

#include <chrono>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>

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
namespace {

#ifdef _WIN32
template <typename T>
void ReleaseCom(T** ptr) {
  if (ptr != nullptr && *ptr != nullptr) {
    (*ptr)->Release();
    *ptr = nullptr;
  }
}

GUID GuidForDecodeOutputSubtype(const DecodeOutputSubtype subtype) {
  switch (subtype) {
    case DecodeOutputSubtype::kNv12:
      return MFVideoFormat_NV12;
    case DecodeOutputSubtype::kArgb32:
      return MFVideoFormat_ARGB32;
    case DecodeOutputSubtype::kRgb32:
    default:
      return MFVideoFormat_RGB32;
  }
}

bool SupportsGpuNv12ShaderResources(ID3D11Device* const device) {
  if (device == nullptr) {
    return false;
  }

  D3D11_TEXTURE2D_DESC textureDesc{};
  textureDesc.Width = 2;
  textureDesc.Height = 2;
  textureDesc.MipLevels = 1;
  textureDesc.ArraySize = 1;
  textureDesc.Format = DXGI_FORMAT_NV12;
  textureDesc.SampleDesc.Count = 1;
  textureDesc.Usage = D3D11_USAGE_DEFAULT;
  textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  ID3D11Texture2D* texture = nullptr;
  HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &texture);
  if (FAILED(hr) || texture == nullptr) {
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC ySrvDesc{};
  ySrvDesc.Format = DXGI_FORMAT_R8_UNORM;
  ySrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  ySrvDesc.Texture2DArray.MostDetailedMip = 0;
  ySrvDesc.Texture2DArray.MipLevels = 1;
  ySrvDesc.Texture2DArray.FirstArraySlice = 0;
  ySrvDesc.Texture2DArray.ArraySize = 1;

  ID3D11ShaderResourceView* ySrv = nullptr;
  hr = device->CreateShaderResourceView(texture, &ySrvDesc, &ySrv);
  if (FAILED(hr) || ySrv == nullptr) {
    ReleaseCom(&texture);
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC uvSrvDesc{};
  uvSrvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
  uvSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  uvSrvDesc.Texture2DArray.MostDetailedMip = 0;
  uvSrvDesc.Texture2DArray.MipLevels = 1;
  uvSrvDesc.Texture2DArray.FirstArraySlice = 0;
  uvSrvDesc.Texture2DArray.ArraySize = 1;

  ID3D11ShaderResourceView* uvSrv = nullptr;
  hr = device->CreateShaderResourceView(texture, &uvSrvDesc, &uvSrv);
  const bool supported = SUCCEEDED(hr) && uvSrv != nullptr;
  ReleaseCom(&uvSrv);
  ReleaseCom(&ySrv);
  ReleaseCom(&texture);
  return supported;
}

int ExtractSourceFrameRateHint(IMFMediaType* const mediaType) {
  if (mediaType == nullptr) {
    return 0;
  }
  UINT32 numerator = 0;
  UINT32 denominator = 0;
  if (FAILED(MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &numerator, &denominator)) ||
      denominator == 0) {
    return 0;
  }
  const double fpsHint = static_cast<double>(numerator) / static_cast<double>(denominator);
  return NormalizeSourceFrameRateHint(fpsHint);
}

class DecodePipelineStub;

class AsyncSourceReaderCallback final : public IMFSourceReaderCallback {
 public:
  explicit AsyncSourceReaderCallback(DecodePipelineStub* owner) : owner_(owner) {}

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags,
                            LONGLONG timestamp100ns, IMFSample* sample) override;
  STDMETHODIMP OnFlush(DWORD) override { return S_OK; }
  STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }

  void Detach() { owner_.store(nullptr, std::memory_order_release); }

 private:
  ~AsyncSourceReaderCallback() = default;

  volatile long refCount_ = 1;
  std::atomic<DecodePipelineStub*> owner_{nullptr};
};
#endif

class DecodePipelineStub final : public IDecodePipeline {
 public:
  ~DecodePipelineStub() override {
    std::lock_guard<std::mutex> lock(mu_);
    ResetStateLocked();
#ifdef _WIN32
    if (mfStarted_) {
      MFShutdown();
      mfStarted_ = false;
    }
#endif
  }

  bool Open(const std::string& path, const DecodeOpenProfile& profile) override {
    std::lock_guard<std::mutex> lock(mu_);

    ResetStateLocked();
    openProfile_ = profile;

    const bool hasPath = !path.empty();
    if (hasPath && !std::filesystem::exists(path)) {
      opened_ = false;
      running_ = false;
      return false;
    }

    mode_ = Mode::kFallbackTicker;
#ifdef _WIN32
    // 有路径时优先尝试 MF Source Reader；失败时退回内置动态源，保证始终有动态效果。
    if (hasPath && TryOpenMediaFoundationLocked(path)) {
      mode_ = Mode::kMediaFoundation;
    }
#endif

    opened_ = true;
    running_ = false;
    sequence_ = 0;
    timelineStart_ = Clock::time_point{};
    pauseAt_ = Clock::time_point{};
    path_ = path;
    return true;
  }

  bool Start() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (!opened_) {
      return false;
    }
    if (!running_) {
      constexpr std::chrono::nanoseconds kFrameIntervalNs(33333333);
      const auto now = Clock::now();
      // 恢复时把暂停区间从时间轴中扣除，避免 sequence/timestamp 跳变。
      if (pauseAt_ != Clock::time_point{}) {
        timelineStart_ += (now - pauseAt_);
        pauseAt_ = Clock::time_point{};
      }
      if (timelineStart_ == Clock::time_point{}) {
        // 首次启动时将时间轴预热 1 帧，保证恢复后能尽快呈现首帧。
        timelineStart_ = now - kFrameIntervalNs;
      }
      running_ = true;
    }
#ifdef _WIN32
    if (mode_ == Mode::kMediaFoundation) {
      ResumeDecodeAsyncRead(&decodeAsyncReadState_);
      PumpDecodeAsyncReadsLocked();
    }
#endif
    return true;
  }

  void Pause() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) {
      pauseAt_ = Clock::now();
    }
    running_ = false;
#ifdef _WIN32
    PauseDecodeAsyncRead(&decodeAsyncReadState_);
#endif
  }

  void Stop() override {
    std::lock_guard<std::mutex> lock(mu_);
    ResetStateLocked();
  }

  void TrimMemory() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) {
      // 运行态不做破坏性动作，避免动态壁纸出现可见闪断。
      return;
    }
    frame_bridge::ClearLatestFrame();
  }

  bool TryAcquireLatestFrame(FrameToken* frame) override {
    if (frame == nullptr) {
      return false;
    }

#ifdef _WIN32
    bool useMediaFoundation = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!opened_ || !running_) {
        return false;
      }
      useMediaFoundation = mode_ == Mode::kMediaFoundation;
      if (!useMediaFoundation) {
        return TryAcquireFallbackFrameLocked(frame);
      }
    }
    return TryAcquireMediaFoundationFrame(frame);
#else
    std::lock_guard<std::mutex> lock(mu_);
    if (!opened_ || !running_) {
      return false;
    }
    return TryAcquireFallbackFrameLocked(frame);
#endif
  }

  void SetFrameReadyNotifier(const DecodeFrameReadyNotifyFn notifyFn,
                             void* const context) override {
    std::lock_guard<std::mutex> lock(mu_);
    frameReadyNotifyFn_ = notifyFn;
    frameReadyNotifyContext_ = context;
  }

  [[nodiscard]] bool SupportsFrameReadyNotifier() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return mode_ == Mode::kMediaFoundation;
  }

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
#endif

  bool TryAcquireFallbackFrameLocked(FrameToken* frame) {
    constexpr std::chrono::nanoseconds kFrameIntervalNs(33333333);
    const auto elapsed = Clock::now() - timelineStart_;
    const auto expectedIndex = static_cast<std::uint64_t>(elapsed / kFrameIntervalNs);
    if (expectedIndex <= sequence_) {
      return false;
    }

    sequence_ = expectedIndex;
    frame->sequence = sequence_;
    frame->width = 0;
    frame->height = 0;
    frame->decodeMode = DecodeMode::kFallbackTicker;
    frame->decodePath = DecodePath::kFallbackTicker;
    frame->gpuBacked = false;
    frame->cpuCopyBytes = 0;
    // 100ns 单位，便于未来与 MF 时间戳对齐。
    frame->timestamp100ns = static_cast<std::int64_t>(sequence_ * 333333);
    frame->sourceFrameRateHint = 0;
    return true;
  }

#ifdef _WIN32
  static void QueryDesktopFrameHint(UINT32* outWidth, UINT32* outHeight) {
    if (outWidth == nullptr || outHeight == nullptr) {
      return;
    }
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width > 0 && height > 0) {
      *outWidth = static_cast<UINT32>(width);
      *outHeight = static_cast<UINT32>(height);
      return;
    }
    *outWidth = 0;
    *outHeight = 0;
  }

  bool TryOpenMediaFoundationLocked(const std::string& path) {
    if (!EnsureMfStartupLocked()) {
      return false;
    }

    const std::filesystem::path fsPath(path);
    const std::wstring widePath = fsPath.wstring();
    if (widePath.empty()) {
      return false;
    }

    auto* callback = new (std::nothrow) AsyncSourceReaderCallback(this);
    if (callback == nullptr) {
      return false;
    }

    const auto createReader = [&](const bool enableVideoProcessing,
                                  const bool tryD3DInterop,
                                  IMFSourceReader** const outReader) -> bool {
      if (outReader == nullptr) {
        return false;
      }
      *outReader = nullptr;

      bool useD3DInterop = false;
      IMFAttributes* readerAttributes = nullptr;
      HRESULT hr = MFCreateAttributes(&readerAttributes, 5);
      if (SUCCEEDED(hr) && readerAttributes != nullptr) {
        // 低延迟模式可减少解码链路内部排队帧数，从而降低内存峰值。
        hr = readerAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
      }
      if (SUCCEEDED(hr) && readerAttributes != nullptr) {
        hr = readerAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback);
      }
      if (SUCCEEDED(hr) && readerAttributes != nullptr && openProfile_.preferHardwareTransforms) {
        // 优先启用硬件变换路径，降低色彩转换的 CPU 占用。
        hr = readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
      }
#if defined(MF_READWRITE_USE_ONLY_HARDWARE_TRANSFORMS)
      if (SUCCEEDED(hr) && readerAttributes != nullptr && openProfile_.requireHardwareTransforms) {
        hr = readerAttributes->SetUINT32(MF_READWRITE_USE_ONLY_HARDWARE_TRANSFORMS, TRUE);
      }
#endif
      if (SUCCEEDED(hr) && readerAttributes != nullptr && enableVideoProcessing) {
        // 回退路径：在无法直接协商输出时再开启软件视频处理。
        hr = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
      }
      if (SUCCEEDED(hr) && readerAttributes != nullptr && tryD3DInterop) {
        ID3D11Device* sharedDevice = d3d11_interop::AcquireSharedDevice();
        if (sharedDevice != nullptr) {
          if (dxgiDeviceManager_ == nullptr &&
              FAILED(MFCreateDXGIDeviceManager(&dxgiDeviceResetToken_, &dxgiDeviceManager_))) {
            dxgiDeviceManager_ = nullptr;
          }
          if (dxgiDeviceManager_ != nullptr &&
              SUCCEEDED(dxgiDeviceManager_->ResetDevice(sharedDevice, dxgiDeviceResetToken_))) {
            if (SUCCEEDED(readerAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER,
                                                       dxgiDeviceManager_))) {
              useD3DInterop = true;
              mfGpuNv12RenderingSupported_ = SupportsGpuNv12ShaderResources(sharedDevice);
            }
          }
          sharedDevice->Release();
        }
      }
      if (FAILED(hr)) {
        if (readerAttributes != nullptr) {
          readerAttributes->Release();
        }
        return false;
      }

      IMFSourceReader* reader = nullptr;
      hr = MFCreateSourceReaderFromURL(widePath.c_str(), readerAttributes, &reader);
      if (readerAttributes != nullptr) {
        readerAttributes->Release();
      }
      if (FAILED(hr) || reader == nullptr) {
        return false;
      }
      mfD3DInteropEnabled_ = useD3DInterop;
      *outReader = reader;
      return true;
    };

    const auto configureReader = [&](IMFSourceReader* const reader, UINT32* const outWidth,
                                     UINT32* const outHeight) -> bool {
      if (reader == nullptr || outWidth == nullptr || outHeight == nullptr) {
        return false;
      }
      // 仅保留视频流，避免不必要的音频流缓存与转换开销。
      HRESULT hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
      if (SUCCEEDED(hr)) {
        hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
      }
      if (FAILED(hr)) {
        return false;
      }

      const auto setOutType = [&](const bool withDesktopHint, const GUID& subtype) -> HRESULT {
        IMFMediaType* outType = nullptr;
        HRESULT localHr = MFCreateMediaType(&outType);
        if (SUCCEEDED(localHr)) {
          localHr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        }
        if (SUCCEEDED(localHr)) {
          localHr = outType->SetGUID(MF_MT_SUBTYPE, subtype);
        }
        if (SUCCEEDED(localHr) && withDesktopHint) {
          UINT32 hintWidth = 0;
          UINT32 hintHeight = 0;
          QueryDesktopFrameHint(&hintWidth, &hintHeight);
          if (hintWidth > 0 && hintHeight > 0) {
            const bool gpuZeroCopySubtype =
                mfD3DInteropEnabled_ &&
                (IsEqualGUID(subtype, MFVideoFormat_ARGB32) ||
                 (mfGpuNv12RenderingSupported_ && IsEqualGUID(subtype, MFVideoFormat_NV12)));
            const bool cpuFallbackPath = !gpuZeroCopySubtype;
            DecodeOutputOptions outputOptions;
            outputOptions.desktopWidth = hintWidth;
            outputOptions.desktopHeight = hintHeight;
            outputOptions.adaptiveQualityEnabled = openProfile_.adaptiveQualityEnabled;
            outputOptions.cpuFallbackPath = cpuFallbackPath;
            outputOptions.longRunLoadLevel = openProfile_.longRunLoadLevel;
            const DecodeOutputHint selectedHint = SelectDecodeOutputHint(outputOptions);
            if (selectedHint.width > 0 && selectedHint.height > 0) {
              // 在 CPU 回退链路对输出像素做上限控制，直接压低解码/上传成本。
              localHr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, selectedHint.width,
                                           selectedHint.height);
            }
          }
        }
        if (SUCCEEDED(localHr)) {
          localHr =
              reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
        }
        if (outType != nullptr) {
          outType->Release();
        }
        return localHr;
      };

      const auto preferredSubtypes =
          BuildPreferredDecodeOutputSubtypes(mfD3DInteropEnabled_, mfGpuNv12RenderingSupported_);
      bool configured = false;
      for (const DecodeOutputSubtype preferredSubtype : preferredSubtypes) {
        const GUID subtype = GuidForDecodeOutputSubtype(preferredSubtype);
        hr = setOutType(true, subtype);
        if (FAILED(hr)) {
          // 某些编解码链路不接受帧大小提示，回退到默认输出协商。
          hr = setOutType(false, subtype);
        }
        if (SUCCEEDED(hr)) {
          selectedOutputSubtype_ = subtype;
          configured = true;
          break;
        }
      }
      if (!configured) {
        return false;
      }

      IMFMediaType* outType = nullptr;
      hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outType);
      if (FAILED(hr) || outType == nullptr) {
        return false;
      }
      const int sourceFrameRateHint = ExtractSourceFrameRateHint(outType);
      hr = MFGetAttributeSize(outType, MF_MT_FRAME_SIZE, outWidth, outHeight);
      outType->Release();
      sourceFrameRateHint_ = sourceFrameRateHint;
      return SUCCEEDED(hr) && *outWidth > 0 && *outHeight > 0;
    };

    UINT32 width = 0;
    UINT32 height = 0;
    IMFSourceReader* reader = nullptr;
    bool opened = createReader(false, openProfile_.preferHardwareTransforms, &reader) &&
                  configureReader(reader, &width, &height);
    if (!opened) {
      if (reader != nullptr) {
        reader->Release();
        reader = nullptr;
      }
      if (!openProfile_.requireHardwareTransforms) {
        // 某些设备/编码器必须启用软件视频处理才能协商到 RGB32。
        opened = createReader(true, false, &reader) && configureReader(reader, &width, &height);
      }
    }
    if (!opened || reader == nullptr) {
      if (reader != nullptr) {
        reader->Release();
      }
      callback->Detach();
      callback->Release();
      return false;
    }

    sourceReader_ = reader;
    sourceReaderCallback_ = callback;
    frameWidth_ = width;
    frameHeight_ = height;
    frameStride_ = IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_NV12) ? width : width * 4;
    mfGpuZeroCopyActive_ = mfD3DInteropEnabled_;
    mfBaseOffset100ns_ = 0;
    mfLastRawTimestamp100ns_ = -1;
    mfLastOutputTimestamp100ns_ = -1;
    ResetDecodeAsyncRead(&decodeAsyncReadState_);
    ClearAsyncReadySampleLocked();
    return true;
  }

  bool EnsureMfStartupLocked() {
    if (mfStarted_) {
      return true;
    }
    const HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) {
      return false;
    }
    mfStarted_ = true;
    return true;
  }

  void ReleaseMfLocked() {
    ClearAsyncReadySampleLocked();
    if (sourceReader_ != nullptr) {
      sourceReader_->Release();
      sourceReader_ = nullptr;
    }
    if (sourceReaderCallback_ != nullptr) {
      sourceReaderCallback_->Detach();
      sourceReaderCallback_->Release();
      sourceReaderCallback_ = nullptr;
    }
    if (dxgiDeviceManager_ != nullptr) {
      dxgiDeviceManager_->Release();
      dxgiDeviceManager_ = nullptr;
    }
    // 保持 MF runtime 常驻进程生命周期，避免频繁 Stop/Open 或退出时的明显卡顿。
    frameWidth_ = 0;
    frameHeight_ = 0;
    frameStride_ = 0;
    mfD3DInteropEnabled_ = false;
    mfGpuZeroCopyActive_ = false;
    mfGpuNv12RenderingSupported_ = false;
    selectedOutputSubtype_ = GUID{};
    mfBaseOffset100ns_ = 0;
    mfLastRawTimestamp100ns_ = -1;
    mfLastOutputTimestamp100ns_ = -1;
    sourceFrameRateHint_ = 0;
    ResetDecodeAsyncRead(&decodeAsyncReadState_);
  }

  bool SeekReaderToStartLocked() {
    if (sourceReader_ == nullptr) {
      return false;
    }
    const GUID kNullGuid = GUID{};
    PROPVARIANT seekPos;
    PropVariantInit(&seekPos);
    seekPos.vt = VT_I8;
    seekPos.hVal.QuadPart = 0;
    const HRESULT hr = sourceReader_->SetCurrentPosition(kNullGuid, seekPos);
    PropVariantClear(&seekPos);
    return SUCCEEDED(hr);
  }

  struct PublishResult final {
    bool ok = false;
    bool gpuBacked = false;
    std::size_t cpuCopyBytes = 0;
  };

  void ClearAsyncReadySampleLocked() {
    if (asyncReadySample_ != nullptr) {
      asyncReadySample_->Release();
      asyncReadySample_ = nullptr;
    }
    asyncReadyRawTimestamp100ns_ = 0;
  }

  bool IssueAsyncReadLocked() {
    if (sourceReader_ == nullptr) {
      return false;
    }
    const HRESULT hr =
        sourceReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr,
                                  nullptr);
    if (FAILED(hr)) {
      return false;
    }
    MarkDecodeAsyncReadIssued(&decodeAsyncReadState_);
    return true;
  }

  void PumpDecodeAsyncReadsLocked() {
    while (sourceReader_ != nullptr) {
      const DecodeAsyncReadAction action = PeekDecodeAsyncReadAction(decodeAsyncReadState_);
      if (action == DecodeAsyncReadAction::kNone) {
        return;
      }
      if (action == DecodeAsyncReadAction::kIssueRead) {
        if (!IssueAsyncReadLocked()) {
          return;
        }
        return;
      }
      if (action == DecodeAsyncReadAction::kSeekToStart) {
        if (!SeekReaderToStartLocked()) {
          return;
        }
        MarkDecodeAsyncReadSeekCompleted(&decodeAsyncReadState_);
        continue;
      }
    }
  }

  void HandleAsyncReadSample(HRESULT status, DWORD streamFlags, LONGLONG rawTimestamp100ns,
                             IMFSample* sample) {
    DecodeFrameReadyNotifyFn notifyFn = nullptr;
    void* notifyContext = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (mode_ != Mode::kMediaFoundation || sourceReader_ == nullptr) {
        return;
      }

      if (FAILED(status)) {
        MarkDecodeAsyncReadCompleted(false, false, &decodeAsyncReadState_);
        return;
      }

      if ((streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
        ClearAsyncReadySampleLocked();
        MarkDecodeAsyncReadCompleted(false, true, &decodeAsyncReadState_);
        PumpDecodeAsyncReadsLocked();
        return;
      }

      if (sample != nullptr && (streamFlags & MF_SOURCE_READERF_STREAMTICK) == 0) {
        ClearAsyncReadySampleLocked();
        sample->AddRef();
        asyncReadySample_ = sample;
        asyncReadyRawTimestamp100ns_ = rawTimestamp100ns;
        MarkDecodeAsyncReadCompleted(true, false, &decodeAsyncReadState_);
        notifyFn = frameReadyNotifyFn_;
        notifyContext = frameReadyNotifyContext_;
      } else {
        MarkDecodeAsyncReadCompleted(false, false, &decodeAsyncReadState_);
        PumpDecodeAsyncReadsLocked();
      }
    }

    // 回调到上层仅用于触发解码泵唤醒，避免无帧状态下的周期性忙轮询。
    if (notifyFn != nullptr) {
      notifyFn(notifyContext);
    }
  }

  [[nodiscard]] static bool IsSelectedOutputNv12(const GUID& selectedOutputSubtype) {
    return IsEqualGUID(selectedOutputSubtype, MFVideoFormat_NV12);
  }

  [[nodiscard]] static DecodePath DecodePathForSelectedSubtype(const GUID& selectedOutputSubtype) {
    return IsSelectedOutputNv12(selectedOutputSubtype) ? DecodePath::kCpuNv12Fallback
                                                       : DecodePath::kCpuRgb32Fallback;
  }

  bool TryTakeReadySampleSnapshotLocked(ReadySampleSnapshot* snapshot) {
    if (snapshot == nullptr || sourceReader_ == nullptr) {
      return false;
    }

    PumpDecodeAsyncReadsLocked();
    if (asyncReadySample_ == nullptr || !decodeAsyncReadState_.sampleReady) {
      return false;
    }

    IMFSample* sample = asyncReadySample_;
    asyncReadySample_ = nullptr;
    const LONGLONG rawTimestamp100ns = asyncReadyRawTimestamp100ns_;
    asyncReadyRawTimestamp100ns_ = 0;
    MarkDecodeAsyncReadSampleConsumed(&decodeAsyncReadState_);
    if (ShouldIssueReadImmediatelyAfterConsume()) {
      PumpDecodeAsyncReadsLocked();
    }

    if (mfLastRawTimestamp100ns_ >= 0 && rawTimestamp100ns < mfLastRawTimestamp100ns_) {
      // 文件循环或时间戳回绕时，增加基线偏移，保证对上层输出时间戳始终单调。
      const std::int64_t nextBaseline =
          mfLastOutputTimestamp100ns_ >= 0 ? mfLastOutputTimestamp100ns_ + 333333 : 0;
      mfBaseOffset100ns_ = nextBaseline - rawTimestamp100ns;
    }

    const std::int64_t outputTimestamp100ns = rawTimestamp100ns + mfBaseOffset100ns_;
    if (mfLastOutputTimestamp100ns_ >= 0 &&
        outputTimestamp100ns <= mfLastOutputTimestamp100ns_) {
      sample->Release();
      return false;
    }

    ++sequence_;
    mfLastRawTimestamp100ns_ = rawTimestamp100ns;
    mfLastOutputTimestamp100ns_ = outputTimestamp100ns;

    snapshot->sample = sample;
    snapshot->rawTimestamp100ns = rawTimestamp100ns;
    snapshot->outputTimestamp100ns = outputTimestamp100ns;
    snapshot->sequence = sequence_;
    snapshot->frameWidth = frameWidth_;
    snapshot->frameHeight = frameHeight_;
    snapshot->frameStride = frameStride_;
    snapshot->mfGpuZeroCopyActive = mfGpuZeroCopyActive_;
    snapshot->selectedOutputSubtype = selectedOutputSubtype_;
    return true;
  }

  PublishResult PublishSampleToBridgeUnlocked(IMFSample* sample,
                                              const ReadySampleSnapshot& snapshot) {
    if (sample == nullptr || snapshot.frameWidth == 0 || snapshot.frameHeight == 0 ||
        snapshot.frameStride == 0) {
      return {};
    }

    if (snapshot.mfGpuZeroCopyActive) {
      IMFMediaBuffer* indexedBuffer = nullptr;
      if (SUCCEEDED(sample->GetBufferByIndex(0, &indexedBuffer)) && indexedBuffer != nullptr) {
        IMFDXGIBuffer* dxgiBuffer = nullptr;
        if (SUCCEEDED(indexedBuffer->QueryInterface(__uuidof(IMFDXGIBuffer),
                                                    reinterpret_cast<void**>(&dxgiBuffer))) &&
            dxgiBuffer != nullptr) {
          ID3D11Texture2D* texture = nullptr;
          UINT subresourceIndex = 0;
          const HRESULT getResourceHr =
              dxgiBuffer->GetResource(__uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(&texture));
          const HRESULT getSubresourceHr = dxgiBuffer->GetSubresourceIndex(&subresourceIndex);
          if (SUCCEEDED(getResourceHr) && SUCCEEDED(getSubresourceHr) && texture != nullptr) {
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            std::shared_ptr<void> textureHolder(
                texture, [](void* ptr) {
                  if (ptr != nullptr) {
                    static_cast<ID3D11Texture2D*>(ptr)->Release();
                  }
                });
            frame_bridge::PublishLatestGpuFrame(
                static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameHeight),
                snapshot.outputTimestamp100ns, snapshot.sequence,
                static_cast<std::uint32_t>(desc.Format), subresourceIndex,
                std::move(textureHolder));
            dxgiBuffer->Release();
            indexedBuffer->Release();
            return PublishResult{true, true, 0};
          }
          if (texture != nullptr) {
            texture->Release();
          }
          dxgiBuffer->Release();
        }
        indexedBuffer->Release();
      }
    }

    if (IsSelectedOutputNv12(snapshot.selectedOutputSubtype)) {
      DWORD sampleBufferCount = 0;
      DWORD sampleTotalLength = 0;
      const bool singleBufferSample =
          SUCCEEDED(sample->GetBufferCount(&sampleBufferCount)) && sampleBufferCount == 1;
      (void)sample->GetTotalLength(&sampleTotalLength);

      IMFMediaBuffer* indexedBuffer = nullptr;
      if (singleBufferSample && SUCCEEDED(sample->GetBufferByIndex(0, &indexedBuffer)) &&
          indexedBuffer != nullptr) {
        IMF2DBuffer* buffer2d = nullptr;
        if (SUCCEEDED(indexedBuffer->QueryInterface(__uuidof(IMF2DBuffer),
                                                    reinterpret_cast<void**>(&buffer2d))) &&
            buffer2d != nullptr) {
          BYTE* scanline0 = nullptr;
          LONG pitch = 0;
          if (SUCCEEDED(buffer2d->Lock2D(&scanline0, &pitch)) && scanline0 != nullptr &&
              pitch > 0) {
            const Nv12Layout layout =
                ComputeNv12Layout(snapshot.frameHeight, static_cast<std::uint32_t>(pitch),
                                  static_cast<std::size_t>(sampleTotalLength));
            if (layout.yPlaneBytes != 0 && layout.uvPlaneBytes != 0) {
              std::shared_ptr<void> bufferHolder(
                  indexedBuffer, [buffer2d](void* ptr) {
                    if (buffer2d != nullptr) {
                      buffer2d->Unlock2D();
                      buffer2d->Release();
                    }
                    if (ptr != nullptr) {
                      static_cast<IMFMediaBuffer*>(ptr)->Release();
                    }
                  });
              frame_bridge::PublishLatestNv12FrameView(
                  static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameHeight),
                  static_cast<int>(pitch), static_cast<int>(pitch), snapshot.outputTimestamp100ns,
                  snapshot.sequence,
                  static_cast<const std::uint8_t*>(scanline0 + layout.yPlaneOffsetBytes),
                  layout.yPlaneBytes,
                  static_cast<const std::uint8_t*>(scanline0 + layout.uvPlaneOffsetBytes),
                  layout.uvPlaneBytes, std::move(bufferHolder));
              return PublishResult{true, false, layout.yPlaneBytes + layout.uvPlaneBytes};
            }

            buffer2d->Unlock2D();
          }
          buffer2d->Release();
        }
        indexedBuffer->Release();
      }

      IMFMediaBuffer* mediaBuffer = nullptr;
      HRESULT hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
      if (FAILED(hr) || mediaBuffer == nullptr) {
        return {};
      }

      BYTE* rawData = nullptr;
      DWORD maxLength = 0;
      DWORD currentLength = 0;
      hr = mediaBuffer->Lock(&rawData, &maxLength, &currentLength);
      if (FAILED(hr) || rawData == nullptr || currentLength == 0) {
        mediaBuffer->Release();
        return {};
      }

      const std::size_t requiredBytes = static_cast<std::size_t>(currentLength);

      const Nv12Layout layout =
          ComputeNv12Layout(snapshot.frameHeight, snapshot.frameWidth, requiredBytes);
      if (layout.yPlaneBytes == 0 || layout.uvPlaneBytes == 0) {
        mediaBuffer->Unlock();
        mediaBuffer->Release();
        return {};
      }

      std::shared_ptr<void> bufferHolder(
          mediaBuffer, [](void* ptr) {
            if (ptr != nullptr) {
              auto* buffer = static_cast<IMFMediaBuffer*>(ptr);
              buffer->Unlock();
              buffer->Release();
            }
          });

      frame_bridge::PublishLatestNv12FrameView(
          static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameHeight),
          static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameWidth),
          snapshot.outputTimestamp100ns, snapshot.sequence,
          static_cast<const std::uint8_t*>(rawData + layout.yPlaneOffsetBytes), layout.yPlaneBytes,
          static_cast<const std::uint8_t*>(rawData + layout.uvPlaneOffsetBytes), layout.uvPlaneBytes,
          std::move(bufferHolder));
      return PublishResult{true, false, requiredBytes};
    }

    IMFMediaBuffer* mediaBuffer = nullptr;
    HRESULT hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
    if (FAILED(hr) || mediaBuffer == nullptr) {
      return {};
    }

    BYTE* rawData = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    hr = mediaBuffer->Lock(&rawData, &maxLength, &currentLength);
    if (FAILED(hr) || rawData == nullptr || currentLength == 0) {
      mediaBuffer->Release();
      return {};
    }

    const std::size_t requiredBytes = static_cast<std::size_t>(currentLength);

    std::shared_ptr<void> bufferHolder(
        mediaBuffer, [](void* ptr) {
          if (ptr != nullptr) {
            auto* buffer = static_cast<IMFMediaBuffer*>(ptr);
            buffer->Unlock();
            buffer->Release();
          }
        });

    frame_bridge::PublishLatestFrameView(
        static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameHeight),
        static_cast<int>(snapshot.frameStride), snapshot.outputTimestamp100ns, snapshot.sequence,
        static_cast<const std::uint8_t*>(rawData), requiredBytes,
        std::move(bufferHolder));
    return PublishResult{true, false, requiredBytes};
  }

  bool TryAcquireMediaFoundationFrame(FrameToken* frame) {
    if (frame == nullptr) {
      return false;
    }

    ReadySampleSnapshot snapshot;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!opened_ || !running_ || mode_ != Mode::kMediaFoundation || sourceReader_ == nullptr) {
        return false;
      }
      if (!TryTakeReadySampleSnapshotLocked(&snapshot)) {
        return false;
      }
    }

    const PublishResult publishResult = PublishSampleToBridgeUnlocked(snapshot.sample, snapshot);
    snapshot.sample->Release();
    if (!publishResult.ok) {
      return false;
    }

    frame->sequence = snapshot.sequence;
    frame->width = static_cast<int>(snapshot.frameWidth);
    frame->height = static_cast<int>(snapshot.frameHeight);
    frame->decodeMode = DecodeMode::kMediaFoundation;
    frame->timestamp100ns = snapshot.outputTimestamp100ns;
    frame->sourceFrameRateHint = sourceFrameRateHint_;
    frame->gpuBacked = publishResult.gpuBacked;
    frame->cpuCopyBytes = publishResult.cpuCopyBytes;
    frame->decodePath = publishResult.gpuBacked
                            ? DecodePath::kDxvaZeroCopy
                            : DecodePathForSelectedSubtype(snapshot.selectedOutputSubtype);
    return true;
  }
#endif

  void ResetStateLocked() {
    running_ = false;
    opened_ = false;
    timelineStart_ = Clock::time_point{};
    pauseAt_ = Clock::time_point{};
    sequence_ = 0;
    path_.clear();
    mode_ = Mode::kFallbackTicker;
    frame_bridge::ClearLatestFrame();
#ifdef _WIN32
    ReleaseMfLocked();
#endif
  }

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
  bool mfGpuNv12RenderingSupported_ = false;
  GUID selectedOutputSubtype_ = GUID{};
  std::uint32_t frameWidth_ = 0;
  std::uint32_t frameHeight_ = 0;
  std::uint32_t frameStride_ = 0;
  std::int64_t mfBaseOffset100ns_ = 0;
  std::int64_t mfLastRawTimestamp100ns_ = -1;
  std::int64_t mfLastOutputTimestamp100ns_ = -1;
  int sourceFrameRateHint_ = 0;
  DecodeAsyncReadState decodeAsyncReadState_{};
  IMFSample* asyncReadySample_ = nullptr;
  std::int64_t asyncReadyRawTimestamp100ns_ = 0;
#endif
};

#ifdef _WIN32
STDMETHODIMP AsyncSourceReaderCallback::QueryInterface(REFIID riid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }
  *object = nullptr;
  if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFSourceReaderCallback)) {
    *object = static_cast<IMFSourceReaderCallback*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) AsyncSourceReaderCallback::AddRef() {
  return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

STDMETHODIMP_(ULONG) AsyncSourceReaderCallback::Release() {
  const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refCount_));
  if (count == 0) {
    delete this;
  }
  return count;
}

STDMETHODIMP AsyncSourceReaderCallback::OnReadSample(HRESULT status, DWORD streamIndex,
                                                     DWORD streamFlags, LONGLONG timestamp100ns,
                                                     IMFSample* sample) {
  (void)streamIndex;
  DecodePipelineStub* owner = owner_.load(std::memory_order_acquire);
  if (owner == nullptr) {
    return S_OK;
  }
  owner->HandleAsyncReadSample(status, streamFlags, timestamp100ns, sample);
  return S_OK;
}
#endif

}  // namespace

std::unique_ptr<IDecodePipeline> CreateDecodePipeline() {
  return std::make_unique<DecodePipelineStub>();
}

}  // namespace wallpaper
