#pragma once

#include <array>
#include <cstdint>

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
};

[[nodiscard]] std::array<SwapChainPlan, 5> BuildSwapChainPlans() noexcept;

}  // namespace wallpaper
