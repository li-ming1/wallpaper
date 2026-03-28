#include "wallpaper/probe_cadence_policy.h"

namespace wallpaper {

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
                                 std::chrono::milliseconds(260)};
  }
  return RuntimeProbeIntervals{std::chrono::milliseconds(450),
                               std::chrono::milliseconds(180)};
}

}  // namespace wallpaper
