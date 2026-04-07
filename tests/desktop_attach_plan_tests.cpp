#include "wallpaper/desktop_attach_policy.h"

#include "test_support.h"

TEST_CASE(DesktopAttachPlan_ContainsWorkerAndDefViewInPriorityOrder) {
  const auto plan = wallpaper::BuildDesktopAttachOrder(true, true);

  EXPECT_EQ(plan.count, static_cast<std::size_t>(2));
  EXPECT_EQ(static_cast<int>(plan.targets[0]),
            static_cast<int>(wallpaper::DesktopAttachTarget::kWorkerW));
  EXPECT_EQ(static_cast<int>(plan.targets[1]),
            static_cast<int>(wallpaper::DesktopAttachTarget::kDefView));
}

TEST_CASE(DesktopAttachPlan_OmitsMissingTargetsWithoutHeapContainer) {
  const auto plan = wallpaper::BuildDesktopAttachOrder(false, true);

  EXPECT_EQ(plan.count, static_cast<std::size_t>(1));
  EXPECT_EQ(static_cast<int>(plan.targets[0]),
            static_cast<int>(wallpaper::DesktopAttachTarget::kDefView));
}
