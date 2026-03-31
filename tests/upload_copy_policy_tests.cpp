#include "wallpaper/upload_copy_policy.h"

#include "test_support.h"

TEST_CASE(UploadCopyPolicy_RgbaRowCopyBytesPreferActivePixelWidth) {
  const std::uint32_t copyBytes =
      wallpaper::SelectRgbaUploadRowCopyBytes(1920, 8192, 8192);
  EXPECT_EQ(copyBytes, 7680U);
}

TEST_CASE(UploadCopyPolicy_RgbaRowCopyBytesClampsToPitchLimit) {
  const std::uint32_t copyBytes =
      wallpaper::SelectRgbaUploadRowCopyBytes(1920, 7000, 7600);
  EXPECT_EQ(copyBytes, 7000U);
}

TEST_CASE(UploadCopyPolicy_Nv12RowCopyBytesPreferActivePixelWidth) {
  const std::uint32_t copyBytes =
      wallpaper::SelectNv12UploadRowCopyBytes(1920, 2048, 2048);
  EXPECT_EQ(copyBytes, 1920U);
}

TEST_CASE(UploadCopyPolicy_Nv12RowCopyBytesClampsToPitchLimit) {
  const std::uint32_t copyBytes =
      wallpaper::SelectNv12UploadRowCopyBytes(1920, 1800, 2048);
  EXPECT_EQ(copyBytes, 1800U);
}

TEST_CASE(UploadCopyPolicy_ContiguousCopyRequiresPitchAndCopyBytesMatch) {
  EXPECT_TRUE(wallpaper::ShouldCopyRowsAsSingleContiguousBlock(10, 2048, 2048, 2048));
  EXPECT_TRUE(!wallpaper::ShouldCopyRowsAsSingleContiguousBlock(0, 2048, 2048, 2048));
  EXPECT_TRUE(!wallpaper::ShouldCopyRowsAsSingleContiguousBlock(10, 2048, 1984, 1984));
}
