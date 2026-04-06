#include "wallpaper/playback_profile_policy.h"

#include "test_support.h"

TEST_CASE(PlaybackProfilePolicy_BalancedForcesMemoryBiasedPreferenceAndAllowsBalancedTuning) {
  EXPECT_TRUE(
      !wallpaper::ResolvePreferHardwareTransformsForPlaybackProfile(
          wallpaper::PlaybackProfile::kBalanced, false));
  EXPECT_TRUE(
      !wallpaper::ResolvePreferHardwareTransformsForPlaybackProfile(
          wallpaper::PlaybackProfile::kBalanced, true));
  EXPECT_TRUE(
      wallpaper::ShouldAllowLongRunDecodeTrimForPlaybackProfile(
          wallpaper::PlaybackProfile::kBalanced));
  EXPECT_TRUE(
      wallpaper::ShouldAllowWorkingSetTrimForPlaybackProfile(
          wallpaper::PlaybackProfile::kBalanced));
  EXPECT_TRUE(
      wallpaper::ShouldAllowCpuFallbackRetuneForPlaybackProfile(
          wallpaper::PlaybackProfile::kBalanced));
}

TEST_CASE(PlaybackProfilePolicy_LowCpuForcesHardwarePreferenceAndSkipsMemoryBiasedTuning) {
  EXPECT_TRUE(
      wallpaper::ResolvePreferHardwareTransformsForPlaybackProfile(
          wallpaper::PlaybackProfile::kLowCpu, false));
  EXPECT_TRUE(
      wallpaper::ResolvePreferHardwareTransformsForPlaybackProfile(
          wallpaper::PlaybackProfile::kLowCpu, true));
  EXPECT_TRUE(
      !wallpaper::ShouldAllowLongRunDecodeTrimForPlaybackProfile(
          wallpaper::PlaybackProfile::kLowCpu));
  EXPECT_TRUE(
      !wallpaper::ShouldAllowWorkingSetTrimForPlaybackProfile(
          wallpaper::PlaybackProfile::kLowCpu));
  EXPECT_TRUE(
      !wallpaper::ShouldAllowCpuFallbackRetuneForPlaybackProfile(
          wallpaper::PlaybackProfile::kLowCpu));
}
