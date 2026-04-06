#include "wallpaper/swap_chain_policy.h"

#include "test_support.h"

TEST_CASE(SwapChainPolicy_PrefersFlipDiscardPlanFirst) {
  const auto plans = wallpaper::BuildSwapChainPlans();

  EXPECT_EQ(plans[0].bufferCount, 2U);
  EXPECT_EQ(plans[0].effect, wallpaper::SwapChainEffect::kFlipDiscard);
}

TEST_CASE(SwapChainPolicy_PreservesFlipAndBltFallbackOrder) {
  const auto plans = wallpaper::BuildSwapChainPlans();

  EXPECT_EQ(plans[1].bufferCount, 2U);
  EXPECT_EQ(plans[1].effect, wallpaper::SwapChainEffect::kFlipDiscard);

  EXPECT_EQ(plans[2].bufferCount, 2U);
  EXPECT_EQ(plans[2].effect, wallpaper::SwapChainEffect::kFlipSequential);

  EXPECT_EQ(plans[3].bufferCount, 1U);
  EXPECT_EQ(plans[3].effect, wallpaper::SwapChainEffect::kDiscard);

  EXPECT_EQ(plans[4].bufferCount, 1U);
  EXPECT_EQ(plans[4].effect, wallpaper::SwapChainEffect::kSequential);
}
