#include "wallpaper/loop_sleep_policy.h"

#include <algorithm>

namespace wallpaper {

int ComputeMainLoopSleepMs(const bool shouldPause, const bool hasActiveVideo,
                           const std::chrono::milliseconds untilNextRender) noexcept {
  if (shouldPause) {
    return 90;
  }
  if (!hasActiveVideo) {
    return 70;
  }

  const auto waitMs = static_cast<int>(untilNextRender.count());
  return std::clamp(waitMs, 2, 40);
}

bool ShouldUseMainLoopMessageAwareWait(const bool shouldPause,
                                       const bool hasActiveVideo) noexcept {
  if (shouldPause) {
    return true;
  }
  if (!hasActiveVideo) {
    return true;
  }
  return false;
}

int ComputeDecodePumpSleepMs(const bool decodeReady, const bool frameAcquired,
                             const int previousSleepMs,
                             const bool frameReadyNotifierAvailable) noexcept {
  if (!decodeReady) {
    return 70;
  }
  if (frameAcquired) {
    return 2;
  }
  if (previousSleepMs < 2) {
    return 2;
  }
  const int maxBackoffMs = frameReadyNotifierAvailable ? 64 : 24;
  const int boundedPreviousSleepMs = std::clamp(previousSleepMs, 2, maxBackoffMs);
  if (boundedPreviousSleepMs <= 4) {
    return std::clamp(boundedPreviousSleepMs * 2, 2, maxBackoffMs);
  }
  return std::clamp(boundedPreviousSleepMs + 4, 2, maxBackoffMs);
}

int ComputeDecodePumpHotSleepMs(const int renderFpsCap, const int sourceFps) noexcept {
  const int effectiveSourceFps = sourceFps > 0 ? sourceFps : renderFpsCap;
  if (effectiveSourceFps >= 60 && renderFpsCap >= 60) {
    return 14;
  }
  if (effectiveSourceFps >= 30) {
    return 28;
  }
  if (effectiveSourceFps >= 25) {
    return 34;
  }
  if (effectiveSourceFps >= 24) {
    return 36;
  }
  return 40;
}

int ClampDecodePumpHotSleepForRealtime(const int requestedHotSleepMs, const int renderFpsCap,
                                       const int sourceFps) noexcept {
  const int effectiveSourceFps = std::clamp(sourceFps > 0 ? sourceFps : renderFpsCap, 1, 240);
  const int frameIntervalMs = std::max(1, 1000 / effectiveSourceFps);
  // 预留 8ms 安全余量，进一步提升解码节拍，优先保证体感速度接近真实 1x。
  const int realtimeSafeMaxSleepMs = std::max(6, frameIntervalMs - 8);
  return std::clamp(requestedHotSleepMs, 6, realtimeSafeMaxSleepMs);
}

int ClampRenderFpsForCompactCpuFallback(const int requestedFpsCap, const DecodePath decodePath,
                                        const std::size_t decodeOutputPixels) noexcept {
  constexpr std::size_t kCompactCpuFallbackPixels = 960U * 540U;
  if (!IsCpuFallbackDecodePath(decodePath) || decodeOutputPixels == 0 ||
      decodeOutputPixels > kCompactCpuFallbackPixels) {
    return requestedFpsCap;
  }
  // 540p 质量档下实际新帧速率本来就明显低于 24fps；继续维持 24fps render cap 只会
  // 增加重复 present 与后续 trim 开销。对该档位单独降到 16fps，更贴近真实新帧节奏。
  if (decodeOutputPixels == kCompactCpuFallbackPixels) {
    return std::min(requestedFpsCap, 16);
  }
  return std::min(requestedFpsCap, 24);
}

int ResolveAutoTargetFps(const int sourceFps) noexcept {
  if (sourceFps <= 0) {
    return 60;
  }
  return NormalizeFpsCap(sourceFps);
}

bool ShouldWakeDecodePumpForRenderCapUpdate(const int previousHotSleepMs, const int nextHotSleepMs,
                                            const int previousFpsCap,
                                            const int nextFpsCap) noexcept {
  return previousHotSleepMs != nextHotSleepMs || previousFpsCap != nextFpsCap;
}

bool ShouldNotifyDecodePumpWake(const bool wakeAlreadyRequested) noexcept {
  return !wakeAlreadyRequested;
}

bool ShouldPreferEventDrivenDecodePumpWait(const bool frameReadyNotifierAvailable,
                                           const bool decodeReady,
                                           const bool frameAcquired) noexcept {
  (void)frameAcquired;
  return frameReadyNotifierAvailable && decodeReady;
}

int SelectDecodePumpInterruptibleWaitMs(const int requestedSleepMs,
                                        const bool preferEventDrivenWait,
                                        const bool frameAcquired) noexcept {
  const int boundedSleepMs = std::clamp(requestedSleepMs, 1, 500);
  if (!preferEventDrivenWait) {
    return boundedSleepMs;
  }
  const int minWaitMs = frameAcquired ? 70 : 140;
  return std::max(boundedSleepMs, minWaitMs);
}

bool ShouldDeferDecodePumpAcquire(const bool frameReadyNotifierAvailable,
                                  const std::uint64_t latestDecodedSequence,
                                  const std::uint64_t latestPresentedSequence) noexcept {
  return frameReadyNotifierAvailable && latestDecodedSequence != 0 &&
         latestDecodedSequence > latestPresentedSequence;
}

bool ShouldUseHighResolutionTimer(const bool hasActiveVideo, const bool stablePaused,
                                  const int appliedFpsCap, const int longRunLoadLevel,
                                  const DecodePath decodePath,
                                  const bool warmupActive) noexcept {
  if (!hasActiveVideo || stablePaused) {
    return false;
  }
  if (appliedFpsCap < 60) {
    return false;
  }
  if (longRunLoadLevel > 0) {
    return false;
  }
  return !IsCpuFallbackDecodePath(decodePath) || warmupActive;
}

}  // namespace wallpaper
