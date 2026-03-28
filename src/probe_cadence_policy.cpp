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

}  // namespace wallpaper
