#include "wallpaper/swap_chain_policy.h"

namespace wallpaper {

std::array<SwapChainPlan, 5> BuildSwapChainPlans(
    const bool allowFrameLatencyWaitableObject) noexcept {
  return {{
      SwapChainPlan{2U, SwapChainEffect::kFlipDiscard, allowFrameLatencyWaitableObject},
      SwapChainPlan{2U, SwapChainEffect::kFlipDiscard, false},
      SwapChainPlan{2U, SwapChainEffect::kFlipSequential, false},
      SwapChainPlan{1U, SwapChainEffect::kDiscard, false},
      SwapChainPlan{1U, SwapChainEffect::kSequential, false},
  }};
}

}  // namespace wallpaper
