#include "wallpaper/decode_output_subtype_policy.h"

#include "test_support.h"

TEST_CASE(DecodeOutputSubtypePolicy_PrefersGpuNv12WhenInteropCanRenderIt) {
  const auto subtypes = wallpaper::BuildPreferredDecodeOutputSubtypes(true, true);
  EXPECT_EQ(subtypes.size(), 3U);
  EXPECT_EQ(subtypes[0], wallpaper::DecodeOutputSubtype::kNv12);
  EXPECT_EQ(subtypes[1], wallpaper::DecodeOutputSubtype::kArgb32);
  EXPECT_EQ(subtypes[2], wallpaper::DecodeOutputSubtype::kRgb32);
}

TEST_CASE(DecodeOutputSubtypePolicy_FallsBackToArgbWhenGpuNv12Unsupported) {
  const auto subtypes = wallpaper::BuildPreferredDecodeOutputSubtypes(true, false);
  EXPECT_EQ(subtypes.size(), 2U);
  EXPECT_EQ(subtypes[0], wallpaper::DecodeOutputSubtype::kArgb32);
  EXPECT_EQ(subtypes[1], wallpaper::DecodeOutputSubtype::kRgb32);
}

TEST_CASE(DecodeOutputSubtypePolicy_CpuPathPrefersNv12ThenRgb32) {
  const auto subtypes = wallpaper::BuildPreferredDecodeOutputSubtypes(false, false);
  EXPECT_EQ(subtypes.size(), 2U);
  EXPECT_EQ(subtypes[0], wallpaper::DecodeOutputSubtype::kNv12);
  EXPECT_EQ(subtypes[1], wallpaper::DecodeOutputSubtype::kRgb32);
}
