#include "wallpaper/cpp26_feature_support.h"

#include "test_support.h"

TEST_CASE(Cpp26FeatureSupport_ExpectedIsAvailable) {
  EXPECT_TRUE(wallpaper::cpp26::kExpected);
}

TEST_CASE(Cpp26FeatureSupport_IfConstevalIsAvailable) {
  EXPECT_TRUE(wallpaper::cpp26::kIfConsteval);
}
