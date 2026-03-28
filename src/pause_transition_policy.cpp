#include "wallpaper/pause_transition_policy.h"

namespace wallpaper {

bool UpdatePauseTransition(const bool rawPause, const std::chrono::steady_clock::time_point now,
                           const std::chrono::milliseconds enterDelay,
                           const std::chrono::milliseconds exitDelay,
                           PauseTransitionState* const state) noexcept {
  if (state == nullptr) {
    return rawPause;
  }

  if (!state->initialized) {
    state->initialized = true;
    state->stablePause = false;
    state->lastRawPause = rawPause;
    state->lastRawChangeAt = now;
    return state->stablePause;
  }

  if (rawPause != state->lastRawPause) {
    state->lastRawPause = rawPause;
    state->lastRawChangeAt = now;
  }

  if (rawPause) {
    if (!state->stablePause && (now - state->lastRawChangeAt) >= enterDelay) {
      state->stablePause = true;
    }
    return state->stablePause;
  }

  if (state->stablePause && (now - state->lastRawChangeAt) >= exitDelay) {
    state->stablePause = false;
  }
  return state->stablePause;
}

}  // namespace wallpaper
