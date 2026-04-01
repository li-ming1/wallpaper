#include "wallpaper/upload_texture_policy.h"

#include "test_support.h"

TEST_CASE(UploadTexturePolicy_UsesDefaultTextureUploadForFullQualityCpuFallback) {
  EXPECT_TRUE(wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, false));
  EXPECT_TRUE(wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kCpuRgb32Fallback, 2560U * 1440U, false));
}

TEST_CASE(UploadTexturePolicy_KeepsDynamicUploadForCompactOrScaledPaths) {
  EXPECT_TRUE(!wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U, false));
  EXPECT_TRUE(!wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, true));
}

TEST_CASE(UploadTexturePolicy_DisablesDefaultUploadForNonCpuPathsOrInvalidInput) {
  EXPECT_TRUE(!wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kDxvaZeroCopy, 1920U * 1080U, false));
  EXPECT_TRUE(!wallpaper::ShouldUseDefaultTextureUpload(
      wallpaper::DecodePath::kUnknown, 0, false));
}

TEST_CASE(UploadTexturePolicy_UsesSharedGpuNv12BridgeOnlyForFullQualityNv12Fallback) {
  EXPECT_TRUE(wallpaper::ShouldUseSharedGpuNv12Bridge(
      wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, false));
  EXPECT_TRUE(!wallpaper::ShouldUseSharedGpuNv12Bridge(
      wallpaper::DecodePath::kCpuRgb32Fallback, 1920U * 1080U, false));
  EXPECT_TRUE(!wallpaper::ShouldUseSharedGpuNv12Bridge(
      wallpaper::DecodePath::kCpuNv12Fallback, 960U * 540U, false));
  EXPECT_TRUE(!wallpaper::ShouldUseSharedGpuNv12Bridge(
      wallpaper::DecodePath::kCpuNv12Fallback, 1920U * 1080U, true));
}
