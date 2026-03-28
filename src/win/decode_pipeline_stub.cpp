#include "wallpaper/interfaces.h"
#include "wallpaper/d3d11_interop_device.h"
#include "wallpaper/frame_buffer_policy.h"

#include "wallpaper/frame_bridge.h"

#include <chrono>
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

class DecodePipelineStub final : public IDecodePipeline {
 public:
  bool Open(const std::string& path, CodecPolicy) override {
    std::lock_guard<std::mutex> lock(mu_);

    ResetStateLocked();

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
    return true;
  }

  void Pause() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) {
      pauseAt_ = Clock::now();
    }
    running_ = false;
  }

  void Stop() override {
    std::lock_guard<std::mutex> lock(mu_);
    ResetStateLocked();
  }

  void TrimMemory() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) {
      // 运行态不直接清空桥接帧，改为下一次发布时按当前帧尺寸收缩缓冲，避免可见闪烁。
      trimRequested_ = true;
      return;
    }
    previousPublishedCpuBytes_ = 0;
    frame_bridge::ClearLatestFrame();
    trimRequested_ = false;
  }

  bool TryAcquireLatestFrame(FrameToken* frame) override {
    if (frame == nullptr) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (!opened_ || !running_) {
      return false;
    }

    if (mode_ == Mode::kMediaFoundation) {
#ifdef _WIN32
      return TryAcquireMediaFoundationFrameLocked(frame);
#else
      return false;
#endif
    }
    return TryAcquireFallbackFrameLocked(frame);
  }

 private:
  using Clock = std::chrono::steady_clock;

  enum class Mode {
    kFallbackTicker = 0,
    kMediaFoundation = 1,
  };

  bool TryAcquireFallbackFrameLocked(FrameToken* frame) {
    constexpr std::chrono::nanoseconds kFrameIntervalNs(33333333);
    const auto elapsed = Clock::now() - timelineStart_;
    const auto expectedIndex = static_cast<std::uint64_t>(elapsed / kFrameIntervalNs);
    if (expectedIndex <= sequence_) {
      return false;
    }

    sequence_ = expectedIndex;
    frame->sequence = sequence_;
    frame->decodeMode = DecodeMode::kFallbackTicker;
    frame->decodePath = DecodePath::kFallbackTicker;
    frame->gpuBacked = false;
    frame->cpuCopyBytes = 0;
    // 100ns 单位，便于未来与 MF 时间戳对齐。
    frame->timestamp100ns = static_cast<std::int64_t>(sequence_ * 333333);
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

    const auto createReader = [&](const bool enableVideoProcessing,
                                  const bool tryD3DInterop,
                                  IMFSourceReader** const outReader) -> bool {
      if (outReader == nullptr) {
        return false;
      }
      *outReader = nullptr;

      bool useD3DInterop = false;
      IMFAttributes* readerAttributes = nullptr;
      HRESULT hr = MFCreateAttributes(&readerAttributes, 4);
      if (SUCCEEDED(hr) && readerAttributes != nullptr) {
        // 低延迟模式可减少解码链路内部排队帧数，从而降低内存峰值。
        hr = readerAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
      }
      if (SUCCEEDED(hr) && readerAttributes != nullptr) {
        // 优先启用硬件变换路径，降低色彩转换的 CPU 占用。
        hr = readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
      }
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
            // 提示输出尺寸不超过当前桌面，降低高分辨率视频的解码缓冲占用。
            localHr = MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, hintWidth, hintHeight);
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

      const GUID preferredSubtypes[] = {
          mfD3DInteropEnabled_ ? MFVideoFormat_ARGB32 : MFVideoFormat_RGB32,
          MFVideoFormat_RGB32,
      };

      bool configured = false;
      for (const GUID& subtype : preferredSubtypes) {
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
      hr = MFGetAttributeSize(outType, MF_MT_FRAME_SIZE, outWidth, outHeight);
      outType->Release();
      return SUCCEEDED(hr) && *outWidth > 0 && *outHeight > 0;
    };

    UINT32 width = 0;
    UINT32 height = 0;
    IMFSourceReader* reader = nullptr;
    bool opened = createReader(false, true, &reader) && configureReader(reader, &width, &height);
    if (!opened) {
      if (reader != nullptr) {
        reader->Release();
        reader = nullptr;
      }
      // 某些设备/编码器必须启用软件视频处理才能协商到 RGB32。
      opened = createReader(true, false, &reader) && configureReader(reader, &width, &height);
    }
    if (!opened || reader == nullptr) {
      if (reader != nullptr) {
        reader->Release();
      }
      return false;
    }

    sourceReader_ = reader;
    frameWidth_ = width;
    frameHeight_ = height;
    frameStride_ = width * 4;
    mfGpuZeroCopyActive_ =
        mfD3DInteropEnabled_ && IsEqualGUID(selectedOutputSubtype_, MFVideoFormat_ARGB32);
    mfBaseOffset100ns_ = 0;
    mfLastRawTimestamp100ns_ = -1;
    mfLastOutputTimestamp100ns_ = -1;
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
    if (sourceReader_ != nullptr) {
      sourceReader_->Release();
      sourceReader_ = nullptr;
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

  PublishResult PublishSampleToBridgeLocked(IMFSample* sample, const std::uint64_t sequence,
                                            const std::int64_t timestamp100ns) {
    if (sample == nullptr || frameWidth_ == 0 || frameHeight_ == 0 || frameStride_ == 0) {
      return {};
    }

    if (mfGpuZeroCopyActive_) {
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
                static_cast<int>(frameWidth_), static_cast<int>(frameHeight_), timestamp100ns,
                sequence, static_cast<std::uint32_t>(desc.Format), subresourceIndex,
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
    const std::size_t nextCapacity =
        trimRequested_ ? requiredBytes
                       : DecideFrameBufferCapacity(previousPublishedCpuBytes_, requiredBytes);
    trimRequested_ = false;
    previousPublishedCpuBytes_ = nextCapacity;

    std::shared_ptr<void> bufferHolder(
        mediaBuffer, [](void* ptr) {
          if (ptr != nullptr) {
            auto* buffer = static_cast<IMFMediaBuffer*>(ptr);
            buffer->Unlock();
            buffer->Release();
          }
        });

    frame_bridge::PublishLatestFrameView(
        static_cast<int>(frameWidth_), static_cast<int>(frameHeight_), static_cast<int>(frameStride_),
        timestamp100ns, sequence, static_cast<const std::uint8_t*>(rawData), requiredBytes,
        std::move(bufferHolder));
    return PublishResult{true, false, requiredBytes};
  }

  bool TryAcquireMediaFoundationFrameLocked(FrameToken* frame) {
    if (sourceReader_ == nullptr) {
      return false;
    }

    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG rawTimestamp100ns = 0;
    IMFSample* sample = nullptr;
    const HRESULT hr = sourceReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex,
                                                 &flags, &rawTimestamp100ns, &sample);
    if (FAILED(hr)) {
      if (sample != nullptr) {
        sample->Release();
      }
      return false;
    }

    // 到达尾帧时回绕到 0，维持时间戳单调并形成循环视频行为。
    if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
      if (sample != nullptr) {
        sample->Release();
      }
      if (!SeekReaderToStartLocked()) {
        return false;
      }
      return false;
    }

    if (sample == nullptr || (flags & MF_SOURCE_READERF_STREAMTICK) != 0) {
      if (sample != nullptr) {
        sample->Release();
      }
      return false;
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
    frame->sequence = sequence_;
    frame->decodeMode = DecodeMode::kMediaFoundation;
    frame->timestamp100ns = outputTimestamp100ns;
    mfLastRawTimestamp100ns_ = rawTimestamp100ns;
    mfLastOutputTimestamp100ns_ = outputTimestamp100ns;
    const PublishResult publishResult =
        PublishSampleToBridgeLocked(sample, sequence_, outputTimestamp100ns);
    frame->gpuBacked = publishResult.gpuBacked;
    frame->cpuCopyBytes = publishResult.cpuCopyBytes;
    frame->decodePath = publishResult.gpuBacked ? DecodePath::kDxvaZeroCopy
                                                : DecodePath::kCpuRgb32Fallback;
    sample->Release();
    return publishResult.ok;
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
    previousPublishedCpuBytes_ = 0;
    trimRequested_ = false;
    frame_bridge::ClearLatestFrame();
#ifdef _WIN32
    ReleaseMfLocked();
#endif
  }

  std::mutex mu_;
  bool opened_ = false;
  bool running_ = false;
  std::string path_;
  Mode mode_ = Mode::kFallbackTicker;
  Clock::time_point timelineStart_{};
  Clock::time_point pauseAt_{};
  std::uint64_t sequence_ = 0;
  std::size_t previousPublishedCpuBytes_ = 0;
  bool trimRequested_ = false;

#ifdef _WIN32
  IMFSourceReader* sourceReader_ = nullptr;
  IMFDXGIDeviceManager* dxgiDeviceManager_ = nullptr;
  UINT dxgiDeviceResetToken_ = 0;
  bool mfStarted_ = false;
  bool mfD3DInteropEnabled_ = false;
  bool mfGpuZeroCopyActive_ = false;
  GUID selectedOutputSubtype_ = GUID{};
  std::uint32_t frameWidth_ = 0;
  std::uint32_t frameHeight_ = 0;
  std::uint32_t frameStride_ = 0;
  std::int64_t mfBaseOffset100ns_ = 0;
  std::int64_t mfLastRawTimestamp100ns_ = -1;
  std::int64_t mfLastOutputTimestamp100ns_ = -1;
#endif
};

}  // namespace

std::unique_ptr<IDecodePipeline> CreateDecodePipeline() {
  return std::make_unique<DecodePipelineStub>();
}

}  // namespace wallpaper
