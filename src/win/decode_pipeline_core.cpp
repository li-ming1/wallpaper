#include "decode_pipeline_internal.h"

namespace wallpaper {

DecodePipelineStub::~DecodePipelineStub() {
  std::lock_guard<std::mutex> lock(mu_);
  ResetStateLocked();
#ifdef _WIN32
  if (mfStarted_) {
    MFShutdown();
    mfStarted_ = false;
  }
#endif
}

bool DecodePipelineStub::Open(const std::string& path, const DecodeOpenProfile& profile) {
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

bool DecodePipelineStub::Start() {
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

void DecodePipelineStub::Pause() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) {
    pauseAt_ = Clock::now();
  }
  running_ = false;
#ifdef _WIN32
  PauseDecodeAsyncRead(&decodeAsyncReadState_);
#endif
}

void DecodePipelineStub::Stop() {
  std::lock_guard<std::mutex> lock(mu_);
  ResetStateLocked();
}

void DecodePipelineStub::TrimMemory() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) {
    // 运行态不做破坏性动作，避免动态壁纸出现可见闪断。
    return;
  }
  frame_bridge::ClearLatestFrame();
}

bool DecodePipelineStub::TryAcquireLatestFrame(FrameToken* frame) {
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

void DecodePipelineStub::SetFrameReadyNotifier(const DecodeFrameReadyNotifyFn notifyFn,
                                               void* const context) {
  std::lock_guard<std::mutex> lock(mu_);
  frameReadyNotifyFn_ = notifyFn;
  frameReadyNotifyContext_ = context;
}

bool DecodePipelineStub::SupportsFrameReadyNotifier() const {
  std::lock_guard<std::mutex> lock(mu_);
  return mode_ == Mode::kMediaFoundation;
}

bool DecodePipelineStub::TryAcquireFallbackFrameLocked(FrameToken* frame) {
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
  frame->decodeInteropStage = DecodeInteropStage::kNotAttempted;
  frame->decodeInteropHresult = 0;
  frame->gpuBacked = false;
  frame->cpuCopyBytes = 0;
  // 100ns 单位，便于未来与 MF 时间戳对齐。
  frame->timestamp100ns = static_cast<std::int64_t>(sequence_ * 333333);
  return true;
}

void DecodePipelineStub::ResetStateLocked() {
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

std::unique_ptr<IDecodePipeline> CreateDecodePipeline() {
  return std::make_unique<DecodePipelineStub>();
}

}  // namespace wallpaper
