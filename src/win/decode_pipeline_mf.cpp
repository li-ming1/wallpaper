#include "decode_pipeline_internal.h"

#include "wallpaper/monitor_rect_cache.h"
#include "wallpaper/monitor_layout_policy.h"

namespace wallpaper {

namespace {

}  // namespace

AsyncSourceReaderCallback::AsyncSourceReaderCallback(DecodePipelineStub* owner) : owner_(owner) {}

void AsyncSourceReaderCallback::Detach() {
  owner_.store(nullptr, std::memory_order_release);
}

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

void DecodePipelineStub::QueryDesktopFrameHint(UINT32* outWidth, UINT32* outHeight) {
  if (outWidth == nullptr || outHeight == nullptr) {
    return;
  }

  const DisplayRect virtualDesktop{
      GetSystemMetrics(SM_XVIRTUALSCREEN),
      GetSystemMetrics(SM_YVIRTUALSCREEN),
      GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
      GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN),
  };
  const MonitorRectSnapshot monitorSnapshot = QueryMonitorRectSnapshotCached();
  const DisplayRect effectiveVirtualDesktop =
      monitorSnapshot.virtualDesktop.right > monitorSnapshot.virtualDesktop.left &&
              monitorSnapshot.virtualDesktop.bottom > monitorSnapshot.virtualDesktop.top
          ? monitorSnapshot.virtualDesktop
          : virtualDesktop;
  const DisplaySize hintSize =
      SelectRepeatedFrameRenderSize(effectiveVirtualDesktop, monitorSnapshot.monitors);
  if (hintSize.width > 0 && hintSize.height > 0) {
    *outWidth = static_cast<UINT32>(hintSize.width);
    *outHeight = static_cast<UINT32>(hintSize.height);
    return;
  }
  *outWidth = 0;
  *outHeight = 0;
}

bool DecodePipelineStub::TryOpenMediaFoundationLocked(const std::string& path) {
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

  UINT32 desktopHintWidth = 0;
  UINT32 desktopHintHeight = 0;
  QueryDesktopFrameHint(&desktopHintWidth, &desktopHintHeight);

  DecodeOutputOptions cpuFallbackOutputOptions;
  cpuFallbackOutputOptions.desktopWidth = desktopHintWidth;
  cpuFallbackOutputOptions.desktopHeight = desktopHintHeight;
  cpuFallbackOutputOptions.cpuFallbackPath = true;
  cpuFallbackOutputOptions.longRunLoadLevel = openProfile_.longRunLoadLevel;
  const bool enableAdvancedVideoProcessing =
      ShouldEnableAdvancedVideoProcessing(cpuFallbackOutputOptions, true);
  DecodeInteropStage lastInteropStage = DecodeInteropStage::kNotAttempted;
  std::int32_t lastInteropHresult = 0;

  const auto createReader = [&](const bool enableVideoProcessing, const bool tryD3DInterop,
                                const bool enableAdvancedProcessing,
                                const bool requireD3DInteropBinding,
                                IMFSourceReader** const outReader) -> bool {
    if (outReader == nullptr) {
      return false;
    }
    *outReader = nullptr;

    bool useD3DInterop = false;
    IMFAttributes* readerAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&readerAttributes, 8);
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
    if (SUCCEEDED(hr) && readerAttributes != nullptr && tryD3DInterop) {
      // 显式声明不禁用 DXVA，避免某些驱动配置下退回系统内存路径。
      hr = readerAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);
    }
#if defined(MF_READWRITE_USE_ONLY_HARDWARE_TRANSFORMS)
    if (SUCCEEDED(hr) && readerAttributes != nullptr && openProfile_.requireHardwareTransforms) {
      hr = readerAttributes->SetUINT32(MF_READWRITE_USE_ONLY_HARDWARE_TRANSFORMS, TRUE);
    }
#endif
    const bool useLegacyVideoProcessing =
        ShouldUseLegacySourceReaderVideoProcessing(tryD3DInterop, enableAdvancedProcessing);
    if (SUCCEEDED(hr) && readerAttributes != nullptr && enableVideoProcessing &&
        useLegacyVideoProcessing) {
      // 仅在非 D3D-advanced 路径启用 legacy video processing，避免无意落回系统内存样本。
      hr = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }
    if (SUCCEEDED(hr) && readerAttributes != nullptr && enableAdvancedProcessing) {
      // advanced video processing 能更稳定地执行尺寸 hint（MF_MT_FRAME_SIZE）。
      hr = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    }
    if (SUCCEEDED(hr) && readerAttributes != nullptr && tryD3DInterop) {
      ID3D11Device* sharedDevice = d3d11_interop::AcquireSharedDevice();
      if (sharedDevice != nullptr) {
        if (dxgiDeviceManager_ == nullptr) {
          const HRESULT managerHr =
              MFCreateDXGIDeviceManager(&dxgiDeviceResetToken_, &dxgiDeviceManager_);
          if (FAILED(managerHr)) {
            lastInteropStage = DecodeInteropStage::kDeviceManagerCreateFailed;
            lastInteropHresult = static_cast<std::int32_t>(managerHr);
            dxgiDeviceManager_ = nullptr;
          }
        }
        if (dxgiDeviceManager_ != nullptr) {
          const HRESULT resetHr =
              dxgiDeviceManager_->ResetDevice(sharedDevice, dxgiDeviceResetToken_);
          if (SUCCEEDED(resetHr)) {
            const HRESULT bindHr =
                readerAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, dxgiDeviceManager_);
            if (SUCCEEDED(bindHr)) {
              useD3DInterop = true;
              lastInteropStage = DecodeInteropStage::kEnabled;
              lastInteropHresult = 0;
            } else {
              lastInteropStage = DecodeInteropStage::kD3DManagerBindFailed;
              lastInteropHresult = static_cast<std::int32_t>(bindHr);
            }
          } else {
            lastInteropStage = DecodeInteropStage::kDeviceManagerResetFailed;
            lastInteropHresult = static_cast<std::int32_t>(resetHr);
          }
        }
        sharedDevice->Release();
      } else {
        lastInteropStage = DecodeInteropStage::kSharedDeviceMissing;
        lastInteropHresult = static_cast<std::int32_t>(E_POINTER);
      }
      if (requireD3DInteropBinding && !useD3DInterop) {
        if (readerAttributes != nullptr) {
          readerAttributes->Release();
        }
        return false;
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
                                   UINT32* const outHeight,
                                   bool* const outRetryWithVideoProcessing) -> bool {
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
      if (SUCCEEDED(localHr) && withDesktopHint && desktopHintWidth > 0 && desktopHintHeight > 0) {
        const bool gpuInteropSubtype =
            IsEqualGUID(subtype, MFVideoFormat_ARGB32) || IsEqualGUID(subtype, MFVideoFormat_RGB32);
        const bool cpuFallbackPath = !(mfD3DInteropEnabled_ && gpuInteropSubtype);
        DecodeOutputOptions outputOptions;
        outputOptions.desktopWidth = desktopHintWidth;
        outputOptions.desktopHeight = desktopHintHeight;
        outputOptions.cpuFallbackPath = cpuFallbackPath;
        outputOptions.longRunLoadLevel = openProfile_.longRunLoadLevel;
        const DecodeOutputHint selectedHint = SelectDecodeOutputHint(outputOptions);
        if (selectedHint.width > 0 && selectedHint.height > 0) {
          // 在 CPU 回退链路对输出像素做上限控制，直接压低解码/上传成本。
          localHr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, selectedHint.width,
                                       selectedHint.height);
        }
      }
      if (SUCCEEDED(localHr)) {
        localHr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outType);
      }
      if (outType != nullptr) {
        outType->Release();
      }
      return localHr;
    };

    const auto queryNegotiatedFrameSize = [&](UINT32* const width, UINT32* const height) -> bool {
      if (width == nullptr || height == nullptr) {
        return false;
      }
      IMFMediaType* outType = nullptr;
      HRESULT localHr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outType);
      if (FAILED(localHr) || outType == nullptr) {
        return false;
      }
      localHr = MFGetAttributeSize(outType, MF_MT_FRAME_SIZE, width, height);
      outType->Release();
      return SUCCEEDED(localHr) && *width > 0 && *height > 0;
    };

    GUID preferredSubtypes[2]{};
    int preferredSubtypeCount = 0;
    if (mfD3DInteropEnabled_) {
      preferredSubtypes[preferredSubtypeCount++] = MFVideoFormat_ARGB32;
      preferredSubtypes[preferredSubtypeCount++] = MFVideoFormat_RGB32;
    } else {
      preferredSubtypes[preferredSubtypeCount++] = MFVideoFormat_NV12;
      preferredSubtypes[preferredSubtypeCount++] = MFVideoFormat_RGB32;
    }

    bool configured = false;
    bool retryWithVideoProcessing = false;
    UINT32 negotiatedWidth = 0;
    UINT32 negotiatedHeight = 0;

    for (int i = 0; i < preferredSubtypeCount; ++i) {
      const GUID& subtype = preferredSubtypes[i];
      hr = setOutType(true, subtype);
      if (FAILED(hr)) {
        // 某些编解码链路不接受帧大小提示，回退到默认输出协商。
        hr = setOutType(false, subtype);
      }
      if (FAILED(hr)) {
        continue;
      }
      if (!queryNegotiatedFrameSize(&negotiatedWidth, &negotiatedHeight)) {
        continue;
      }

      selectedOutputSubtype_ = subtype;
      configured = true;
      retryWithVideoProcessing = false;

      if (desktopHintWidth > 0 && desktopHintHeight > 0) {
        const bool gpuInteropSubtype =
            IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_ARGB32) ||
            IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_RGB32);
        const bool cpuFallbackPath = !(mfD3DInteropEnabled_ && gpuInteropSubtype);
        DecodeOutputOptions outputOptions;
        outputOptions.desktopWidth = desktopHintWidth;
        outputOptions.desktopHeight = desktopHintHeight;
        outputOptions.cpuFallbackPath = cpuFallbackPath;
        outputOptions.longRunLoadLevel = openProfile_.longRunLoadLevel;
        retryWithVideoProcessing = ShouldRetryDecodeOpenWithVideoProcessing(
            outputOptions, negotiatedWidth, negotiatedHeight);
      }
      break;
    }

    if (!configured) {
      return false;
    }

    *outWidth = negotiatedWidth;
    *outHeight = negotiatedHeight;
    if (outRetryWithVideoProcessing != nullptr) {
      *outRetryWithVideoProcessing = retryWithVideoProcessing;
    }
    return true;
  };

  UINT32 width = 0;
  UINT32 height = 0;
  IMFSourceReader* reader = nullptr;
  bool retryWithVideoProcessing = false;
  bool attemptedSoftwareFallback = false;
  const bool requireD3DOnHardwareAttempt = ShouldRequireD3DInteropBinding(
      cpuFallbackOutputOptions, openProfile_.preferHardwareTransforms,
      openProfile_.requireHardwareTransforms);
  bool opened = createReader(false, openProfile_.preferHardwareTransforms, false,
                             requireD3DOnHardwareAttempt, &reader) &&
                configureReader(reader, &width, &height, &retryWithVideoProcessing);
  if (opened && retryWithVideoProcessing && !openProfile_.requireHardwareTransforms) {
    if (reader != nullptr) {
      reader->Release();
      reader = nullptr;
    }
    bool retriedWithD3DInterop = false;
    const bool preserveD3DInterop = ShouldPreserveD3DInteropOnVideoProcessingRetry(
        cpuFallbackOutputOptions, openProfile_.preferHardwareTransforms);
    const bool requireD3DOnRetry =
        ShouldRequireD3DInteropBinding(cpuFallbackOutputOptions, preserveD3DInterop, false);
    opened = createReader(true, preserveD3DInterop, enableAdvancedVideoProcessing,
                          requireD3DOnRetry, &reader) &&
             configureReader(reader, &width, &height, &retryWithVideoProcessing);
    retriedWithD3DInterop = preserveD3DInterop;
    if ((!opened || retryWithVideoProcessing) && retriedWithD3DInterop) {
      if (reader != nullptr) {
        reader->Release();
        reader = nullptr;
      }
      opened = createReader(true, false, enableAdvancedVideoProcessing, false, &reader) &&
               configureReader(reader, &width, &height, &retryWithVideoProcessing);
    }
    attemptedSoftwareFallback = true;
  }
  if (!opened || retryWithVideoProcessing) {
    if (reader != nullptr) {
      reader->Release();
      reader = nullptr;
    }
    if (!openProfile_.requireHardwareTransforms && !attemptedSoftwareFallback) {
      // 某些设备/编码器必须启用软件视频处理才能协商到 RGB32。
    opened = createReader(true, openProfile_.preferHardwareTransforms,
                          enableAdvancedVideoProcessing, requireD3DOnHardwareAttempt, &reader) &&
               configureReader(reader, &width, &height, &retryWithVideoProcessing);
      if ((!opened || retryWithVideoProcessing) && openProfile_.preferHardwareTransforms) {
        if (reader != nullptr) {
          reader->Release();
          reader = nullptr;
        }
        // 最后再退回纯软件路径，避免过早放弃 D3D 互操作。
        opened = createReader(true, false, enableAdvancedVideoProcessing, false, &reader) &&
                 configureReader(reader, &width, &height, &retryWithVideoProcessing);
      }
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
  if (mfD3DInteropEnabled_) {
    interopStage_ = DecodeInteropStage::kEnabled;
    interopHresult_ = 0;
  } else if (lastInteropStage == DecodeInteropStage::kEnabled) {
    interopStage_ = DecodeInteropStage::kNotAttempted;
    interopHresult_ = 0;
  } else {
    interopStage_ = lastInteropStage;
    interopHresult_ = lastInteropHresult;
  }

  sourceReader_ = reader;
  sourceReaderCallback_ = callback;
  frameWidth_ = width;
  frameHeight_ = height;
  frameStride_ = IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_NV12) ? width : width * 4;
  mfGpuZeroCopyActive_ =
      mfD3DInteropEnabled_ &&
      (IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_ARGB32) ||
       IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_RGB32));
  mfBaseOffset100ns_ = 0;
  mfLastRawTimestamp100ns_ = -1;
  mfLastOutputTimestamp100ns_ = -1;
  ResetDecodeAsyncRead(&decodeAsyncReadState_);
  ClearAsyncReadySampleLocked();
  return true;
}

bool DecodePipelineStub::EnsureMfStartupLocked() {
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

void DecodePipelineStub::ReleaseMfLocked() {
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
  selectedOutputSubtype_ = GUID{};
  mfBaseOffset100ns_ = 0;
  mfLastRawTimestamp100ns_ = -1;
  mfLastOutputTimestamp100ns_ = -1;
  ResetDecodeAsyncRead(&decodeAsyncReadState_);
  samplePublishCachedStrategy_.store(SamplePublishStrategy::kUnknown, std::memory_order_release);
  interopStage_ = DecodeInteropStage::kNotAttempted;
  interopHresult_ = 0;
  ReleaseSharedNv12BridgeTexturesLocked();
  ReleaseSharedNv12BridgeDeviceCacheLocked();
}

void DecodePipelineStub::ReleaseSharedNv12BridgeTexturesLocked() {
  if (sharedNv12BridgeYTexture_ != nullptr) {
    sharedNv12BridgeYTexture_->Release();
    sharedNv12BridgeYTexture_ = nullptr;
  }
  if (sharedNv12BridgeUvTexture_ != nullptr) {
    sharedNv12BridgeUvTexture_->Release();
    sharedNv12BridgeUvTexture_ = nullptr;
  }
  sharedNv12BridgeWidth_ = 0;
  sharedNv12BridgeHeight_ = 0;
}

bool DecodePipelineStub::EnsureSharedNv12BridgeDeviceCacheLocked() {
  const SharedDeviceCacheDecision decision = ResolveSharedDeviceCacheDecision(
      sharedNv12BridgeDevice_ != nullptr, sharedNv12BridgeContext_ != nullptr,
      sharedNv12BridgeDeviceRevision_, d3d11_interop::QuerySharedDeviceRevision());
  if (!decision.refreshRequired) {
    return true;
  }

  ID3D11Device* sharedDevice = d3d11_interop::AcquireSharedDevice();
  if (sharedDevice == nullptr) {
    if (decision.releaseBridgeTextures) {
      ReleaseSharedNv12BridgeTexturesLocked();
    }
    ReleaseSharedNv12BridgeDeviceCacheLocked();
    return false;
  }

  ID3D11DeviceContext* sharedContext = nullptr;
  sharedDevice->GetImmediateContext(&sharedContext);
  if (sharedContext == nullptr) {
    sharedDevice->Release();
    if (decision.releaseBridgeTextures) {
      ReleaseSharedNv12BridgeTexturesLocked();
    }
    ReleaseSharedNv12BridgeDeviceCacheLocked();
    return false;
  }

  if (decision.releaseBridgeTextures) {
    ReleaseSharedNv12BridgeTexturesLocked();
  }
  ReleaseSharedNv12BridgeDeviceCacheLocked();
  sharedNv12BridgeDevice_ = sharedDevice;
  sharedNv12BridgeContext_ = sharedContext;
  sharedNv12BridgeDeviceRevision_ = d3d11_interop::QuerySharedDeviceRevision();
  return true;
}

void DecodePipelineStub::ReleaseSharedNv12BridgeDeviceCacheLocked() {
  if (sharedNv12BridgeContext_ != nullptr) {
    sharedNv12BridgeContext_->Release();
    sharedNv12BridgeContext_ = nullptr;
  }
  if (sharedNv12BridgeDevice_ != nullptr) {
    sharedNv12BridgeDevice_->Release();
    sharedNv12BridgeDevice_ = nullptr;
  }
  sharedNv12BridgeDeviceRevision_ = 0;
}

bool DecodePipelineStub::EnsureSharedNv12BridgeTexturesLocked(ID3D11Device* const device,
                                                              const UINT width,
                                                              const UINT height) {
  if (device == nullptr || width == 0 || height == 0 || (width & 1U) != 0U || (height & 1U) != 0U) {
    return false;
  }
  if (sharedNv12BridgeYTexture_ != nullptr && sharedNv12BridgeUvTexture_ != nullptr &&
      sharedNv12BridgeWidth_ == width && sharedNv12BridgeHeight_ == height) {
    return true;
  }

  ReleaseSharedNv12BridgeTexturesLocked();

  D3D11_TEXTURE2D_DESC yDesc{};
  yDesc.Width = width;
  yDesc.Height = height;
  yDesc.MipLevels = 1;
  yDesc.ArraySize = 1;
  yDesc.Format = DXGI_FORMAT_R8_UNORM;
  yDesc.SampleDesc.Count = 1;
  yDesc.Usage = D3D11_USAGE_DEFAULT;
  yDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  HRESULT hr = device->CreateTexture2D(&yDesc, nullptr, &sharedNv12BridgeYTexture_);
  if (FAILED(hr) || sharedNv12BridgeYTexture_ == nullptr) {
    ReleaseSharedNv12BridgeTexturesLocked();
    return false;
  }

  D3D11_TEXTURE2D_DESC uvDesc{};
  uvDesc.Width = width / 2U;
  uvDesc.Height = height / 2U;
  uvDesc.MipLevels = 1;
  uvDesc.ArraySize = 1;
  uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
  uvDesc.SampleDesc.Count = 1;
  uvDesc.Usage = D3D11_USAGE_DEFAULT;
  uvDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  hr = device->CreateTexture2D(&uvDesc, nullptr, &sharedNv12BridgeUvTexture_);
  if (FAILED(hr) || sharedNv12BridgeUvTexture_ == nullptr) {
    ReleaseSharedNv12BridgeTexturesLocked();
    return false;
  }

  sharedNv12BridgeWidth_ = width;
  sharedNv12BridgeHeight_ = height;
  return true;
}

bool DecodePipelineStub::SeekReaderToStartLocked() {
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

void DecodePipelineStub::ClearAsyncReadySampleLocked() {
  if (asyncReadySample_ != nullptr) {
    asyncReadySample_->Release();
    asyncReadySample_ = nullptr;
  }
  asyncReadyRawTimestamp100ns_ = 0;
}

bool DecodePipelineStub::IssueAsyncReadLocked() {
  if (sourceReader_ == nullptr) {
    return false;
  }
  const HRESULT hr = sourceReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr,
                                               nullptr, nullptr, nullptr);
  if (FAILED(hr)) {
    return false;
  }
  MarkDecodeAsyncReadIssued(&decodeAsyncReadState_);
  return true;
}

void DecodePipelineStub::PumpDecodeAsyncReadsLocked() {
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

void DecodePipelineStub::HandleAsyncReadSample(HRESULT status, DWORD streamFlags,
                                               LONGLONG rawTimestamp100ns, IMFSample* sample) {
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

bool DecodePipelineStub::IsSelectedOutputNv12(const GUID& selectedOutputSubtype) {
  return IsEqualGUID(selectedOutputSubtype, MFVideoFormat_NV12);
}

DecodePath DecodePipelineStub::DecodePathForSelectedSubtype(const GUID& selectedOutputSubtype) {
  return IsSelectedOutputNv12(selectedOutputSubtype) ? DecodePath::kCpuNv12Fallback
                                                     : DecodePath::kCpuRgb32Fallback;
}

bool DecodePipelineStub::TryTakeReadySampleSnapshotLocked(ReadySampleSnapshot* snapshot) {
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
  if (mfLastOutputTimestamp100ns_ >= 0 && outputTimestamp100ns <= mfLastOutputTimestamp100ns_) {
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

DecodePipelineStub::PublishResult DecodePipelineStub::PublishSampleToBridgeUnlocked(
    IMFSample* sample, const ReadySampleSnapshot& snapshot) {
  if (sample == nullptr || snapshot.frameWidth == 0 || snapshot.frameHeight == 0 ||
      snapshot.frameStride == 0) {
    return {};
  }

  const UploadScalePlan bridgeScalePlan =
      SelectUploadScalePlanForCpuUpload(static_cast<int>(snapshot.frameWidth),
                                        static_cast<int>(snapshot.frameHeight));
  const bool shouldScaleForBridge =
      bridgeScalePlan.targetWidth > 0 && bridgeScalePlan.targetHeight > 0 &&
      (bridgeScalePlan.targetWidth != static_cast<int>(snapshot.frameWidth) ||
       bridgeScalePlan.targetHeight != static_cast<int>(snapshot.frameHeight));

  const auto publishScaledNv12Frame = [&](const std::uint8_t* const yPlaneData,
                                          const int yStrideBytes,
                                          const std::uint8_t* const uvPlaneData,
                                          const int uvStrideBytes) -> PublishResult {
    CompactCpuFrameBuffer scaledBuffer;
    if (!TryDownscaleNv12FrameNearest(yPlaneData, yStrideBytes, uvPlaneData, uvStrideBytes,
                                      static_cast<int>(snapshot.frameWidth),
                                      static_cast<int>(snapshot.frameHeight),
                                      bridgeScalePlan.targetWidth, bridgeScalePlan.targetHeight,
                                      &scaledFrameBufferPool_,
                                      &scaledBuffer)) {
      return {};
    }

    if (scaledBuffer.data == nullptr || scaledBuffer.dataBytes == 0 || scaledBuffer.holder == nullptr) {
      return {};
    }

    const std::size_t yPlaneBytes = scaledBuffer.secondaryPlaneOffsetBytes;
    const std::size_t uvPlaneBytes = scaledBuffer.dataBytes - scaledBuffer.secondaryPlaneOffsetBytes;
    frame_bridge::PublishLatestNv12FrameView(
        scaledBuffer.width, scaledBuffer.height, scaledBuffer.primaryStrideBytes,
        scaledBuffer.secondaryStrideBytes, snapshot.outputTimestamp100ns, snapshot.sequence,
        scaledBuffer.data + scaledBuffer.primaryPlaneOffsetBytes, yPlaneBytes,
        scaledBuffer.data + scaledBuffer.secondaryPlaneOffsetBytes, uvPlaneBytes,
        std::move(scaledBuffer.holder));
    return PublishResult{true, false, scaledBuffer.dataBytes, static_cast<std::uint32_t>(scaledBuffer.width),
                         static_cast<std::uint32_t>(scaledBuffer.height)};
  };

  const auto publishScaledRgbaFrame =
      [&](const std::uint8_t* const rgbaData, const int strideBytes) -> PublishResult {
    CompactCpuFrameBuffer scaledBuffer;
    if (!TryDownscaleRgbaFrameNearest(rgbaData, static_cast<int>(snapshot.frameWidth),
                                      static_cast<int>(snapshot.frameHeight), strideBytes,
                                      bridgeScalePlan.targetWidth, bridgeScalePlan.targetHeight,
                                      &scaledFrameBufferPool_,
                                      &scaledBuffer)) {
      return {};
    }
    if (scaledBuffer.data == nullptr || scaledBuffer.dataBytes == 0 || scaledBuffer.holder == nullptr) {
      return {};
    }

    frame_bridge::PublishLatestFrameView(
        scaledBuffer.width, scaledBuffer.height, scaledBuffer.primaryStrideBytes,
        snapshot.outputTimestamp100ns, snapshot.sequence, scaledBuffer.data, scaledBuffer.dataBytes,
        std::move(scaledBuffer.holder));
    return PublishResult{true, false, scaledBuffer.dataBytes, static_cast<std::uint32_t>(scaledBuffer.width),
                         static_cast<std::uint32_t>(scaledBuffer.height)};
  };

  const auto publishSharedGpuNv12Frame =
      [&](const std::uint8_t* const yPlaneData, const int yStrideBytes,
          const std::size_t yPlaneBytes, const std::uint8_t* const uvPlaneData,
          const int uvStrideBytes, const std::size_t uvPlaneBytes) -> PublishResult {
    const std::size_t decodeOutputPixels =
        static_cast<std::size_t>(snapshot.frameWidth) * static_cast<std::size_t>(snapshot.frameHeight);
    if (!ShouldUseSharedGpuNv12Bridge(DecodePathForSelectedSubtype(snapshot.selectedOutputSubtype),
                                      decodeOutputPixels, shouldScaleForBridge)) {
      return {};
    }

    ID3D11Texture2D* yTexture = nullptr;
    ID3D11Texture2D* uvTexture = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!EnsureSharedNv12BridgeDeviceCacheLocked()) {
        return {};
      }
      if (!EnsureSharedNv12BridgeTexturesLocked(sharedNv12BridgeDevice_, snapshot.frameWidth,
                                                snapshot.frameHeight)) {
        return {};
      }
      sharedNv12BridgeContext_->UpdateSubresource(sharedNv12BridgeYTexture_, 0, nullptr,
                                                  yPlaneData, static_cast<UINT>(yStrideBytes),
                                                  static_cast<UINT>(yStrideBytes) *
                                                      snapshot.frameHeight);
      sharedNv12BridgeContext_->UpdateSubresource(
          sharedNv12BridgeUvTexture_, 0, nullptr, uvPlaneData,
          static_cast<UINT>(uvStrideBytes),
          static_cast<UINT>(uvStrideBytes) * (snapshot.frameHeight / 2U));
      yTexture = sharedNv12BridgeYTexture_;
      uvTexture = sharedNv12BridgeUvTexture_;
      yTexture->AddRef();
      uvTexture->AddRef();
    }

    std::shared_ptr<void> yTextureHolder(yTexture, [](void* ptr) {
      if (ptr != nullptr) {
        static_cast<ID3D11Texture2D*>(ptr)->Release();
      }
    });
    std::shared_ptr<void> uvTextureHolder(uvTexture, [](void* ptr) {
      if (ptr != nullptr) {
        static_cast<ID3D11Texture2D*>(ptr)->Release();
      }
    });
    frame_bridge::PublishLatestGpuNv12Frame(static_cast<int>(snapshot.frameWidth),
                                            static_cast<int>(snapshot.frameHeight),
                                            snapshot.outputTimestamp100ns, snapshot.sequence,
                                            std::move(yTextureHolder), std::move(uvTextureHolder));
    return PublishResult{true, true, yPlaneBytes + uvPlaneBytes, snapshot.frameWidth,
                         snapshot.frameHeight};
  };

  const auto rememberSuccessfulStrategy = [&](const SamplePublishStrategy strategy) {
    samplePublishCachedStrategy_.store(strategy, std::memory_order_release);
  };
  const auto invalidateCachedStrategy = [&]() {
    samplePublishCachedStrategy_.store(SamplePublishStrategy::kUnknown, std::memory_order_release);
  };

  SamplePublishCapabilities publishCapabilities;
  publishCapabilities.gpuZeroCopyActive = snapshot.mfGpuZeroCopyActive;
  publishCapabilities.format = IsSelectedOutputNv12(snapshot.selectedOutputSubtype)
                                   ? SamplePublishFormat::kNv12
                                   : SamplePublishFormat::kRgba;

  SamplePublishStrategyPlan publishPlan;
  const SamplePublishStrategy cachedStrategy =
      samplePublishCachedStrategy_.load(std::memory_order_acquire);
  if (IsSamplePublishStrategyCompatible(cachedStrategy, publishCapabilities)) {
    AppendUniqueSamplePublishStrategy(&publishPlan, cachedStrategy);
  }
  AppendUniqueSamplePublishStrategy(
      &publishPlan, SelectPrimarySamplePublishStrategy(publishCapabilities));
  AppendUniqueSamplePublishStrategy(
      &publishPlan, SelectSecondarySamplePublishStrategy(publishCapabilities));

  const auto publishGpuDxgiFrame = [&]() -> PublishResult {
    IMFMediaBuffer* indexedBuffer = nullptr;
    if (FAILED(sample->GetBufferByIndex(0, &indexedBuffer)) || indexedBuffer == nullptr) {
      return {};
    }

    IMFDXGIBuffer* dxgiBuffer = nullptr;
    if (FAILED(indexedBuffer->QueryInterface(__uuidof(IMFDXGIBuffer),
                                             reinterpret_cast<void**>(&dxgiBuffer))) ||
        dxgiBuffer == nullptr) {
      indexedBuffer->Release();
      return {};
    }

    ID3D11Texture2D* texture = nullptr;
    UINT subresourceIndex = 0;
    const HRESULT getResourceHr =
        dxgiBuffer->GetResource(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
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
      frame_bridge::PublishLatestGpuFrame(static_cast<int>(snapshot.frameWidth),
                                          static_cast<int>(snapshot.frameHeight),
                                          snapshot.outputTimestamp100ns, snapshot.sequence,
                                          static_cast<std::uint32_t>(desc.Format), subresourceIndex,
                                          std::move(textureHolder));
      dxgiBuffer->Release();
      indexedBuffer->Release();
      return PublishResult{true, true, 0, snapshot.frameWidth, snapshot.frameHeight};
    }

    if (texture != nullptr) {
      texture->Release();
    }
    dxgiBuffer->Release();
    indexedBuffer->Release();
    return {};
  };

  const auto publishNv12Lock2DFrame = [&]() -> PublishResult {
    DWORD sampleBufferCount = 0;
    DWORD sampleTotalLength = 0;
    const bool singleBufferSample =
        SUCCEEDED(sample->GetBufferCount(&sampleBufferCount)) && sampleBufferCount == 1;
    (void)sample->GetTotalLength(&sampleTotalLength);

    if (!singleBufferSample) {
      return {};
    }

    IMFMediaBuffer* indexedBuffer = nullptr;
    if (FAILED(sample->GetBufferByIndex(0, &indexedBuffer)) || indexedBuffer == nullptr) {
      return {};
    }

    IMF2DBuffer* buffer2d = nullptr;
    if (FAILED(indexedBuffer->QueryInterface(__uuidof(IMF2DBuffer),
                                             reinterpret_cast<void**>(&buffer2d))) ||
        buffer2d == nullptr) {
      indexedBuffer->Release();
      return {};
    }

    BYTE* scanline0 = nullptr;
    LONG pitch = 0;
    if (SUCCEEDED(buffer2d->Lock2D(&scanline0, &pitch)) && scanline0 != nullptr && pitch > 0) {
      const Nv12Layout layout = ComputeNv12Layout(snapshot.frameHeight,
                                                  static_cast<std::uint32_t>(pitch),
                                                  static_cast<std::size_t>(sampleTotalLength));
      if (layout.yPlaneBytes != 0 && layout.uvPlaneBytes != 0) {
        if (shouldScaleForBridge) {
          const PublishResult scaledResult = publishScaledNv12Frame(
              static_cast<const std::uint8_t*>(scanline0 + layout.yPlaneOffsetBytes),
              static_cast<int>(pitch),
              static_cast<const std::uint8_t*>(scanline0 + layout.uvPlaneOffsetBytes),
              static_cast<int>(pitch));
          if (scaledResult.ok) {
            buffer2d->Unlock2D();
            buffer2d->Release();
            indexedBuffer->Release();
            return scaledResult;
          }
        }

        const PublishResult gpuBridgeResult = publishSharedGpuNv12Frame(
            static_cast<const std::uint8_t*>(scanline0 + layout.yPlaneOffsetBytes),
            static_cast<int>(pitch), layout.yPlaneBytes,
            static_cast<const std::uint8_t*>(scanline0 + layout.uvPlaneOffsetBytes),
            static_cast<int>(pitch), layout.uvPlaneBytes);
        if (gpuBridgeResult.ok) {
          buffer2d->Unlock2D();
          buffer2d->Release();
          indexedBuffer->Release();
          return gpuBridgeResult;
        }

        std::shared_ptr<void> bufferHolder(indexedBuffer, [buffer2d](void* ptr) {
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
        return PublishResult{true, false, layout.yPlaneBytes + layout.uvPlaneBytes,
                             snapshot.frameWidth, snapshot.frameHeight};
      }

      buffer2d->Unlock2D();
    }
    buffer2d->Release();
    indexedBuffer->Release();
    return {};
  };

  class ScopedContiguousSampleBuffer final {
   public:
    explicit ScopedContiguousSampleBuffer(IMFSample* const sample) : sample_(sample) {}

    ~ScopedContiguousSampleBuffer() {
      if (mediaBuffer_ != nullptr) {
        mediaBuffer_->Unlock();
        mediaBuffer_->Release();
      }
    }

    [[nodiscard]] bool EnsureLocked() {
      if (mediaBuffer_ != nullptr) {
        return rawData_ != nullptr && currentLength_ > 0;
      }
      if (sample_ == nullptr) {
        return false;
      }
      if (FAILED(sample_->ConvertToContiguousBuffer(&mediaBuffer_)) || mediaBuffer_ == nullptr) {
        mediaBuffer_ = nullptr;
        return false;
      }

      DWORD maxLength = 0;
      if (FAILED(mediaBuffer_->Lock(&rawData_, &maxLength, &currentLength_)) ||
          rawData_ == nullptr || currentLength_ == 0) {
        mediaBuffer_->Release();
        mediaBuffer_ = nullptr;
        rawData_ = nullptr;
        currentLength_ = 0;
        return false;
      }
      return true;
    }

    [[nodiscard]] const std::uint8_t* data() const noexcept {
      return static_cast<const std::uint8_t*>(rawData_);
    }

    [[nodiscard]] std::size_t size_bytes() const noexcept {
      return static_cast<std::size_t>(currentLength_);
    }

    [[nodiscard]] std::shared_ptr<void> DetachLockedHolder() {
      IMFMediaBuffer* const buffer = mediaBuffer_;
      mediaBuffer_ = nullptr;
      rawData_ = nullptr;
      currentLength_ = 0;
      if (buffer == nullptr) {
        return {};
      }
      return std::shared_ptr<void>(buffer, [](void* ptr) {
        if (ptr != nullptr) {
          auto* typedBuffer = static_cast<IMFMediaBuffer*>(ptr);
          typedBuffer->Unlock();
          typedBuffer->Release();
        }
      });
    }

   private:
    IMFSample* sample_ = nullptr;
    IMFMediaBuffer* mediaBuffer_ = nullptr;
    BYTE* rawData_ = nullptr;
    DWORD currentLength_ = 0;
  };

  ScopedContiguousSampleBuffer contiguousSampleBuffer(sample);

  const auto publishNv12ContiguousFrame = [&]() -> PublishResult {
    if (!contiguousSampleBuffer.EnsureLocked()) {
      return {};
    }

    const std::uint8_t* const rawData = contiguousSampleBuffer.data();
    const std::size_t requiredBytes = contiguousSampleBuffer.size_bytes();

    const Nv12Layout layout =
        ComputeNv12Layout(snapshot.frameHeight, snapshot.frameWidth, requiredBytes);
    if (layout.yPlaneBytes == 0 || layout.uvPlaneBytes == 0) {
      return {};
    }

    if (shouldScaleForBridge) {
      const PublishResult scaledResult =
          publishScaledNv12Frame(static_cast<const std::uint8_t*>(rawData + layout.yPlaneOffsetBytes),
                                 static_cast<int>(snapshot.frameWidth),
                                 static_cast<const std::uint8_t*>(rawData + layout.uvPlaneOffsetBytes),
                                 static_cast<int>(snapshot.frameWidth));
      if (scaledResult.ok) {
        return scaledResult;
      }
    }

    const PublishResult gpuBridgeResult = publishSharedGpuNv12Frame(
        static_cast<const std::uint8_t*>(rawData + layout.yPlaneOffsetBytes),
        static_cast<int>(snapshot.frameWidth), layout.yPlaneBytes,
        static_cast<const std::uint8_t*>(rawData + layout.uvPlaneOffsetBytes),
        static_cast<int>(snapshot.frameWidth), layout.uvPlaneBytes);
    if (gpuBridgeResult.ok) {
      return gpuBridgeResult;
    }

    std::shared_ptr<void> bufferHolder = contiguousSampleBuffer.DetachLockedHolder();
    if (!bufferHolder) {
      return {};
    }
    frame_bridge::PublishLatestNv12FrameView(
        static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameHeight),
        static_cast<int>(snapshot.frameWidth), static_cast<int>(snapshot.frameWidth),
        snapshot.outputTimestamp100ns, snapshot.sequence,
        static_cast<const std::uint8_t*>(rawData + layout.yPlaneOffsetBytes), layout.yPlaneBytes,
        static_cast<const std::uint8_t*>(rawData + layout.uvPlaneOffsetBytes), layout.uvPlaneBytes,
        std::move(bufferHolder));
    return PublishResult{true, false, requiredBytes, snapshot.frameWidth, snapshot.frameHeight};
  };

  const auto publishRgbaContiguousFrame = [&]() -> PublishResult {
    if (!contiguousSampleBuffer.EnsureLocked()) {
      return {};
    }

    const std::uint8_t* const rawData = contiguousSampleBuffer.data();
    const std::size_t requiredBytes = contiguousSampleBuffer.size_bytes();

    if (shouldScaleForBridge) {
      const PublishResult scaledResult = publishScaledRgbaFrame(
          static_cast<const std::uint8_t*>(rawData), static_cast<int>(snapshot.frameStride));
      if (scaledResult.ok) {
        return scaledResult;
      }
    }

    std::shared_ptr<void> bufferHolder = contiguousSampleBuffer.DetachLockedHolder();
    if (!bufferHolder) {
      return {};
    }
    frame_bridge::PublishLatestFrameView(static_cast<int>(snapshot.frameWidth),
                                         static_cast<int>(snapshot.frameHeight),
                                         static_cast<int>(snapshot.frameStride),
                                         snapshot.outputTimestamp100ns, snapshot.sequence,
                                         static_cast<const std::uint8_t*>(rawData), requiredBytes,
                                         std::move(bufferHolder));
    return PublishResult{true, false, requiredBytes, snapshot.frameWidth, snapshot.frameHeight};
  };

  for (std::size_t index = 0; index < publishPlan.count; ++index) {
    const SamplePublishStrategy strategy = publishPlan.strategies[index];
    PublishResult result;
    switch (strategy) {
      case SamplePublishStrategy::kGpuDxgi:
        result = publishGpuDxgiFrame();
        break;
      case SamplePublishStrategy::kNv12Lock2D:
        result = publishNv12Lock2DFrame();
        break;
      case SamplePublishStrategy::kNv12Contiguous:
        result = publishNv12ContiguousFrame();
        break;
      case SamplePublishStrategy::kRgbaContiguous:
        result = publishRgbaContiguousFrame();
        break;
      case SamplePublishStrategy::kUnknown:
      default:
        break;
    }
    if (result.ok) {
      rememberSuccessfulStrategy(strategy);
      return result;
    }
  }

  invalidateCachedStrategy();
  return {};
}

bool DecodePipelineStub::TryAcquireMediaFoundationFrame(FrameToken* frame) {
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
  frame->width = static_cast<int>(publishResult.outputWidth);
  frame->height = static_cast<int>(publishResult.outputHeight);
  frame->decodeMode = DecodeMode::kMediaFoundation;
  frame->timestamp100ns = snapshot.outputTimestamp100ns;
  frame->gpuBacked = publishResult.gpuBacked;
  frame->cpuCopyBytes = publishResult.cpuCopyBytes;
  const bool gpuZeroCopyPath = publishResult.gpuBacked && snapshot.mfGpuZeroCopyActive;
  frame->decodeInteropStage = gpuZeroCopyPath ? DecodeInteropStage::kEnabled : interopStage_;
  frame->decodeInteropHresult = gpuZeroCopyPath ? 0 : interopHresult_;
  frame->decodePath =
      gpuZeroCopyPath ? DecodePath::kDxvaZeroCopy
                      : DecodePathForSelectedSubtype(snapshot.selectedOutputSubtype);
  return true;
}

}  // namespace wallpaper
