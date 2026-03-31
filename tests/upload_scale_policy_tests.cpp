#include "wallpaper/upload_scale_policy.h"

#include "test_support.h"

TEST_CASE(UploadScalePolicy_DoesNotScaleInvalidInput) {
  const wallpaper::UploadScalePlan plan = wallpaper::SelectUploadScalePlanForCpuUpload(0, 1080);
  EXPECT_EQ(plan.targetWidth, 0);
  EXPECT_EQ(plan.targetHeight, 0);
  EXPECT_EQ(plan.divisor, 1);
}

TEST_CASE(UploadScalePolicy_DoesNotScale720pOrLower) {
  const wallpaper::UploadScalePlan plan =
      wallpaper::SelectUploadScalePlanForCpuUpload(1280, 720);
  EXPECT_EQ(plan.targetWidth, 1280);
  EXPECT_EQ(plan.targetHeight, 720);
  EXPECT_EQ(plan.divisor, 1);
}

TEST_CASE(UploadScalePolicy_DoesNotScale1080p) {
  const wallpaper::UploadScalePlan plan =
      wallpaper::SelectUploadScalePlanForCpuUpload(1920, 1080);
  EXPECT_EQ(plan.targetWidth, 1920);
  EXPECT_EQ(plan.targetHeight, 1080);
  EXPECT_EQ(plan.divisor, 1);
}

TEST_CASE(UploadScalePolicy_Scales1440pToHalf) {
  const wallpaper::UploadScalePlan plan =
      wallpaper::SelectUploadScalePlanForCpuUpload(2560, 1440);
  EXPECT_EQ(plan.targetWidth, 1280);
  EXPECT_EQ(plan.targetHeight, 720);
  EXPECT_EQ(plan.divisor, 2);
}

TEST_CASE(UploadScalePolicy_DoesNotScaleOddResolution) {
  const wallpaper::UploadScalePlan plan =
      wallpaper::SelectUploadScalePlanForCpuUpload(1919, 1079);
  EXPECT_EQ(plan.targetWidth, 1919);
  EXPECT_EQ(plan.targetHeight, 1079);
  EXPECT_EQ(plan.divisor, 1);
}
