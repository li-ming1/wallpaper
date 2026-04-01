#include "wallpaper/probe_cadence_policy.h"

namespace wallpaper {

namespace {

constexpr std::size_t kCompactCpuFallbackPixels = 960U * 540U;

}

bool ShouldRefreshRuntimeProbe(const ProbeClock::time_point now,
                               const ProbeClock::time_point lastSampleAt,
                               const std::chrono::milliseconds interval) noexcept {
  if (lastSampleAt == ProbeClock::time_point{}) {
    return true;
  }
  if (now < lastSampleAt) {
    return true;
  }
  return (now - lastSampleAt) >= interval;
}

RuntimeProbeIntervals SelectRuntimeProbeIntervals(const bool stablePaused) noexcept {
  if (stablePaused) {
    return RuntimeProbeIntervals{std::chrono::milliseconds(1200),
                                 std::chrono::milliseconds(180)};
  }
  return RuntimeProbeIntervals{std::chrono::milliseconds(450),
                               std::chrono::milliseconds(180)};
}

int UpdateForegroundProbeFailureStreak(const bool probeSucceeded, const int previousStreak) noexcept {
  if (probeSucceeded) {
    return 0;
  }
  if (previousStreak < 0) {
    return 1;
  }
  if (previousStreak >= 1024) {
    return 1024;
  }
  return previousStreak + 1;
}

bool ShouldUseConservativeDesktopContext(const int failureStreak,
                                         const int failureThreshold) noexcept {
  if (failureThreshold <= 0) {
    return true;
  }
  return failureStreak >= failureThreshold;
}

bool ShouldReuseForegroundProbeResult(const std::uintptr_t currentForegroundWindowHandle,
                                      const std::uintptr_t lastForegroundWindowHandle,
                                      const ProbeClock::time_point now,
                                      const ProbeClock::time_point lastDeepProbeAt,
                                      const std::chrono::milliseconds deepProbeReuseInterval)
    noexcept {
  if (currentForegroundWindowHandle == 0 || currentForegroundWindowHandle != lastForegroundWindowHandle) {
    return false;
  }
  if (lastDeepProbeAt == ProbeClock::time_point{}) {
    return false;
  }
  if (deepProbeReuseInterval <= std::chrono::milliseconds(0)) {
    return false;
  }
  if (now < lastDeepProbeAt) {
    return true;
  }
  return (now - lastDeepProbeAt) < deepProbeReuseInterval;
}

std::chrono::milliseconds SelectSessionProbeIntervalForState(
    const std::chrono::milliseconds baseInterval, const bool sessionInteractive,
    const bool batterySaverActive, const bool remoteSessionActive) noexcept {
  if (baseInterval <= std::chrono::milliseconds(0)) {
    return std::chrono::milliseconds(1);
  }
  if (!sessionInteractive || batterySaverActive || remoteSessionActive) {
    return baseInterval;
  }

  const auto relaxed = baseInterval * 2;
  constexpr auto kRelaxedCap = std::chrono::milliseconds(1200);
  return relaxed > kRelaxedCap ? kRelaxedCap : relaxed;
}

std::chrono::milliseconds SelectMetricsSampleInterval(const bool hasActiveVideo,
                                                      const bool stablePaused,
                                                      const bool occluded) noexcept {
  if (hasActiveVideo && !stablePaused && !occluded) {
    return std::chrono::milliseconds(1000);
  }
  return std::chrono::milliseconds(2000);
}

std::chrono::milliseconds SelectRuntimeMetricsSampleInterval(
    const bool hasActiveVideo, const bool stablePaused, const bool occluded,
    const DecodePath decodePath, const std::size_t decodeOutputPixels,
    const int appliedFpsCap) noexcept {
  const auto baseInterval =
      SelectMetricsSampleInterval(hasActiveVideo, stablePaused, occluded);
  if (baseInterval > std::chrono::milliseconds(1000)) {
    return baseInterval;
  }
  if (appliedFpsCap <= 30 && IsCpuFallbackDecodePath(decodePath) &&
      decodeOutputPixels > kCompactCpuFallbackPixels) {
    return std::chrono::milliseconds(2000);
  }
  return baseInterval;
}

}  // namespace wallpaper
