#pragma once

#include <array>
#include <cstdint>

#include "wallpaper/config.h"

namespace wallpaper {

enum class SwapChainEffect : std::uint8_t {
  kDiscard = 0,
  kSequential = 1,
  kFlipDiscard = 2,
  kFlipSequential = 3,
};

struct SwapChainPlan final {
  std::uint32_t bufferCount = 2;
  SwapChainEffect effect = SwapChainEffect::kFlipDiscard;
  bool useFrameLatencyWaitableObject = false;
};

[[nodiscard]] std::array<SwapChainPlan, 5> BuildSwapChainPlans(
    bool allowFrameLatencyWaitableObject) noexcept;

[[nodiscard]] bool ShouldAllowFrameLatencyWaitableObject(
    FrameLatencyWaitableMode mode) noexcept;

}  // namespace wallpaper
