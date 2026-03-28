#include "wallpaper/desktop_attach_policy.h"

#include "test_support.h"

#include <array>

TEST_CASE(DesktopAttachPolicy_PrefersWorkerWOverDefView) {
  const auto order = wallpaper::BuildDesktopAttachOrder(true, true);
  const std::array<wallpaper::DesktopAttachTarget, 2> expected = {
      wallpaper::DesktopAttachTarget::kWorkerW,
      wallpaper::DesktopAttachTarget::kDefView,
  };
  EXPECT_EQ(order.size(), expected.size());
  EXPECT_EQ(order[0], expected[0]);
  EXPECT_EQ(order[1], expected[1]);
}

TEST_CASE(DesktopAttachPolicy_UsesDefViewWhenWorkerWMissing) {
  const auto order = wallpaper::BuildDesktopAttachOrder(false, true);
  const std::array<wallpaper::DesktopAttachTarget, 1> expected = {
      wallpaper::DesktopAttachTarget::kDefView,
  };
  EXPECT_EQ(order.size(), expected.size());
  EXPECT_EQ(order[0], expected[0]);
}

TEST_CASE(DesktopAttachPolicy_RejectsUnsafeFallbacks) {
  const auto order = wallpaper::BuildDesktopAttachOrder(false, false);
  EXPECT_TRUE(order.empty());
}
