#include "wallpaper/swap_chain_policy.h"

#include "test_support.h"

TEST_CASE(SwapChainPolicy_PrefersFlipDiscardPlanFirst) {
  const auto plans = wallpaper::BuildSwapChainPlans(true);

  EXPECT_EQ(plans[0].bufferCount, 2U);
  EXPECT_EQ(plans[0].effect, wallpaper::SwapChainEffect::kFlipDiscard);
  EXPECT_TRUE(plans[0].useFrameLatencyWaitableObject);
}

TEST_CASE(SwapChainPolicy_PreservesFlipAndBltFallbackOrder) {
  const auto plans = wallpaper::BuildSwapChainPlans(true);

  EXPECT_EQ(plans[1].bufferCount, 2U);
  EXPECT_EQ(plans[1].effect, wallpaper::SwapChainEffect::kFlipDiscard);
  EXPECT_TRUE(!plans[1].useFrameLatencyWaitableObject);

  EXPECT_EQ(plans[2].bufferCount, 2U);
  EXPECT_EQ(plans[2].effect, wallpaper::SwapChainEffect::kFlipSequential);
  EXPECT_TRUE(!plans[2].useFrameLatencyWaitableObject);

  EXPECT_EQ(plans[3].bufferCount, 1U);
  EXPECT_EQ(plans[3].effect, wallpaper::SwapChainEffect::kDiscard);
  EXPECT_TRUE(!plans[3].useFrameLatencyWaitableObject);

  EXPECT_EQ(plans[4].bufferCount, 1U);
  EXPECT_EQ(plans[4].effect, wallpaper::SwapChainEffect::kSequential);
  EXPECT_TRUE(!plans[4].useFrameLatencyWaitableObject);
}

TEST_CASE(SwapChainPolicy_DisablesWaitableObjectWhenNotAllowed) {
  const auto plans = wallpaper::BuildSwapChainPlans(false);

  EXPECT_TRUE(!plans[0].useFrameLatencyWaitableObject);
}
