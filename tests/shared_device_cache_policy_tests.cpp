#include "wallpaper/shared_device_cache_policy.h"

#include "test_support.h"

TEST_CASE(SharedDeviceCachePolicy_StableRevisionReusesCachedBindings) {
  const auto decision =
      wallpaper::ResolveSharedDeviceCacheDecision(true, true, 7U, 7U);

  EXPECT_TRUE(!decision.refreshRequired);
  EXPECT_TRUE(!decision.releaseBridgeTextures);
}

TEST_CASE(SharedDeviceCachePolicy_MissingContextForcesRefresh) {
  const auto decision =
      wallpaper::ResolveSharedDeviceCacheDecision(true, false, 7U, 7U);

  EXPECT_TRUE(decision.refreshRequired);
  EXPECT_TRUE(!decision.releaseBridgeTextures);
}

TEST_CASE(SharedDeviceCachePolicy_RevisionChangeForcesRefreshAndBridgeRebuild) {
  const auto decision =
      wallpaper::ResolveSharedDeviceCacheDecision(true, true, 7U, 8U);

  EXPECT_TRUE(decision.refreshRequired);
  EXPECT_TRUE(decision.releaseBridgeTextures);
}

TEST_CASE(SharedDeviceCachePolicy_FirstAcquireRefreshesWithoutBridgeRelease) {
  const auto decision =
      wallpaper::ResolveSharedDeviceCacheDecision(false, false, 0U, 1U);

  EXPECT_TRUE(decision.refreshRequired);
  EXPECT_TRUE(!decision.releaseBridgeTextures);
}
