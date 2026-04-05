#include "wallpaper/app.h"
#include "wallpaper/loop_sleep_policy.h"

#include <algorithm>
#include <chrono>

#ifdef _WIN32
#define PSAPI_VERSION 1
#include <windows.h>
#endif

namespace wallpaper {

void App::StartDecodePump() {
  if (decodePumpRunning_.exchange(true)) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(decodePumpWaitMu_);
    decodePumpWakeRequested_ = false;
  }

  decodePumpThread_ = std::thread([this]() {
    const auto sleepInterruptible = [this](const int sleepMs, const bool preferEventDrivenWait,
                                           const bool frameAcquired) {
      if (sleepMs <= 0) {
        return;
      }
      const int waitMs =
          SelectDecodePumpInterruptibleWaitMs(sleepMs, preferEventDrivenWait, frameAcquired);
      std::unique_lock<std::mutex> lock(decodePumpWaitMu_);
      decodePumpWaitCv_.wait_for(lock, std::chrono::milliseconds(waitMs),
                                 [this]() {
                                   return !decodePumpRunning_.load() || decodePumpWakeRequested_;
                                 });
      decodePumpWakeRequested_ = false;
    };

#ifdef _WIN32
    int lastDecodeThreadPriority = THREAD_PRIORITY_NORMAL;
    RuntimeThreadQos lastDecodeThreadQos = RuntimeThreadQos::kNormal;
    ULONG lastMemoryPriority = MEMORY_PRIORITY_NORMAL;
    const auto setDecodeThreadPriority = [&](const int priority) {
      if (priority == lastDecodeThreadPriority) {
        return;
      }
      if (SetThreadPriority(GetCurrentThread(), priority) != FALSE) {
        lastDecodeThreadPriority = priority;
      }
    };
    const auto applyDecodeThreadServiceHints = [&](const bool decodeReady,
                                                   const bool warmupActive) {
      const RuntimeThreadQos nextQos =
          warmupActive ? RuntimeThreadQos::kNormal : RuntimeThreadQos::kEco;
      const ULONG nextMemoryPriority =
          warmupActive ? MEMORY_PRIORITY_NORMAL : MEMORY_PRIORITY_LOW;
      decodeThreadQos_.store(static_cast<int>(nextQos));
      if (nextQos != lastDecodeThreadQos) {
        THREAD_POWER_THROTTLING_STATE throttling{};
        throttling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        throttling.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
        throttling.StateMask =
            nextQos == RuntimeThreadQos::kEco ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0;
        SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &throttling,
                             sizeof(throttling));
        lastDecodeThreadQos = nextQos;
      }
      if (nextMemoryPriority != lastMemoryPriority) {
        MEMORY_PRIORITY_INFORMATION memoryInfo{};
        memoryInfo.MemoryPriority = nextMemoryPriority;
        SetThreadInformation(GetCurrentThread(), ThreadMemoryPriority, &memoryInfo,
                             sizeof(memoryInfo));
        lastMemoryPriority = nextMemoryPriority;
      }
      setDecodeThreadPriority(decodeReady ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_IDLE);
    };
    applyDecodeThreadServiceHints(false, false);
#endif

    int decodeIdleSleepMs = 2;
    while (decodePumpRunning_.load()) {
      const bool decodeReady = decodePipeline_ && decodeOpened_.load() && decodeRunning_.load();
      const bool warmupActive = decodeWarmupActive_.load();
#ifdef _WIN32
      applyDecodeThreadServiceHints(decodeReady, warmupActive);
#else
      decodeThreadQos_.store(static_cast<int>(warmupActive ? RuntimeThreadQos::kNormal
                                                           : RuntimeThreadQos::kEco));
#endif
      if (!decodeReady) {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(false, false, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  false),
            false);
        continue;
      }
      if (decodeIdleSleepMs > 2) {
        decodeIdleSleepMs = 2;
      }

      const std::uint64_t latestDecodedSequence = latestDecodedSequence_.load(std::memory_order_acquire);
      const std::uint64_t latestPresentedSequence =
          latestPresentedSequence_.load(std::memory_order_acquire);
      if (ShouldDeferDecodePumpAcquire(decodeFrameReadyNotifierAvailable_, latestDecodedSequence,
                                       latestPresentedSequence)) {
        const int hotSleepMs = std::max(decodePumpHotSleepMs_.load(), decodeIdleSleepMs);
        sleepInterruptible(hotSleepMs, false, true);
        continue;
      }

      FrameToken token{};
      if (decodePipeline_->TryAcquireLatestFrame(&token)) {
        std::lock_guard<std::mutex> lock(decodedTokenMu_);
        latestDecodedToken_ = token;
        hasLatestDecodedToken_ = true;
        latestDecodedSequence_.store(token.sequence, std::memory_order_release);
#ifdef _WIN32
        if (decodeFrameReadyEvent_ != nullptr) {
          (void)SetEvent(reinterpret_cast<HANDLE>(decodeFrameReadyEvent_));
        }
#endif
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, true, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        const int hotSleepMs = decodePumpHotSleepMs_.load();
        if (hotSleepMs > decodeIdleSleepMs) {
          decodeIdleSleepMs = hotSleepMs;
        }
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  true),
            true);
      } else {
        decodeIdleSleepMs = ComputeDecodePumpSleepMs(true, false, decodeIdleSleepMs,
                                                     decodeFrameReadyNotifierAvailable_);
        sleepInterruptible(
            decodeIdleSleepMs,
            ShouldPreferEventDrivenDecodePumpWait(decodeFrameReadyNotifierAvailable_, decodeReady,
                                                  false),
            false);
      }
    }
  });
}

void App::StopDecodePump() {
  if (!decodePumpRunning_.exchange(false)) {
    return;
  }
  WakeDecodePump();
  if (decodePumpThread_.joinable()) {
    decodePumpThread_.join();
  }
}

void App::WakeDecodePump() {
  bool shouldNotify = false;
  {
    std::lock_guard<std::mutex> lock(decodePumpWaitMu_);
    shouldNotify = ShouldNotifyDecodePumpWake(decodePumpWakeRequested_);
    if (shouldNotify) {
      decodePumpWakeRequested_ = true;
    }
  }
  if (!shouldNotify) {
    return;
  }
  decodePumpWaitCv_.notify_one();
}

void App::OnDecodeFrameReady() { WakeDecodePump(); }

void App::OnDecodeFrameReadyThunk(void* const context) {
  if (context == nullptr) {
    return;
  }
  static_cast<App*>(context)->OnDecodeFrameReady();
}

}  // namespace wallpaper
