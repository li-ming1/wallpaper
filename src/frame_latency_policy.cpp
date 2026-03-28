#include "wallpaper/frame_latency_policy.h"

namespace wallpaper {

bool ShouldSkipPresentOnWaitTimeout(const bool waitableEnabled, const bool gateArmed,
                                    const int timeoutSkips,
                                    const int maxTimeoutSkipsBeforeForce) noexcept {
  if (!waitableEnabled || !gateArmed) {
    return false;
  }
  if (maxTimeoutSkipsBeforeForce <= 0) {
    return false;
  }
  return timeoutSkips < maxTimeoutSkipsBeforeForce;
}

}  // namespace wallpaper
