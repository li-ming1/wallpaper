#pragma once

#include "wallpaper/config.h"

namespace wallpaper {

[[nodiscard]] constexpr bool ResolvePreferHardwareTransformsForPlaybackProfile(
    const PlaybackProfile profile, const bool tunedPreference) noexcept {
  if (profile == PlaybackProfile::kLowCpu) {
    return true;
  }
  // balanced 档持续偏向较低驻留的 decode open 选择，
  // 不再沿用调用方默认的硬件 transform 偏好，避免启动阶段与 low_cpu 同相。
  (void)tunedPreference;
  return false;
}

[[nodiscard]] constexpr bool ShouldAllowLongRunDecodeTrimForPlaybackProfile(
    const PlaybackProfile profile) noexcept {
  return profile == PlaybackProfile::kBalanced;
}

[[nodiscard]] constexpr bool ShouldAllowWorkingSetTrimForPlaybackProfile(
    const PlaybackProfile profile) noexcept {
  return profile == PlaybackProfile::kBalanced;
}

[[nodiscard]] constexpr bool ShouldAllowCpuFallbackRetuneForPlaybackProfile(
    const PlaybackProfile profile) noexcept {
  return profile == PlaybackProfile::kBalanced;
}

}  // namespace wallpaper
