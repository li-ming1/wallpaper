#include "wallpaper/swap_chain_policy.h"

namespace wallpaper {

std::array<SwapChainPlan, 5> BuildSwapChainPlans() noexcept {
  return {{
      SwapChainPlan{2U, SwapChainEffect::kFlipDiscard},
      SwapChainPlan{2U, SwapChainEffect::kFlipDiscard},
      SwapChainPlan{2U, SwapChainEffect::kFlipSequential},
      SwapChainPlan{1U, SwapChainEffect::kDiscard},
      SwapChainPlan{1U, SwapChainEffect::kSequential},
  }};
}

}  // namespace wallpaper
