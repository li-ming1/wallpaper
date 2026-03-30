#include "wallpaper/video_render_policy.h"

#include "test_support.h"

TEST_CASE(VideoRenderPolicy_UsesHighQualityUpscaleOnlyWhenTargetExceedsSource) {
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(1920, 1080, 1920, 1080));
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(1920, 1080, 1280, 720));
  EXPECT_TRUE(wallpaper::ShouldUseHighQualityUpscale(1920, 1080, 2560, 1440));
  EXPECT_TRUE(wallpaper::ShouldUseHighQualityUpscale(1280, 720, 1280, 800));
}

TEST_CASE(VideoRenderPolicy_InvalidSizesDisableHighQualityUpscale) {
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(0, 1080, 1920, 1080));
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(1920, 0, 1920, 1080));
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(1920, 1080, 0, 1080));
  EXPECT_TRUE(!wallpaper::ShouldUseHighQualityUpscale(1920, 1080, 1920, 0));
}

TEST_CASE(VideoRenderPolicy_SelectsBt709ForHdFrames) {
  EXPECT_EQ(wallpaper::SelectVideoColorSpace(1280, 720),
            wallpaper::VideoColorSpace::kBt709Limited);
  EXPECT_EQ(wallpaper::SelectVideoColorSpace(1920, 1080),
            wallpaper::VideoColorSpace::kBt709Limited);
}

TEST_CASE(VideoRenderPolicy_SelectsBt601ForSdFrames) {
  EXPECT_EQ(wallpaper::SelectVideoColorSpace(640, 480),
            wallpaper::VideoColorSpace::kBt601Limited);
  EXPECT_EQ(wallpaper::SelectVideoColorSpace(720, 576),
            wallpaper::VideoColorSpace::kBt601Limited);
}
