#include "wallpaper/monitor_layout_policy.h"

#include "test_support.h"

namespace {

wallpaper::DisplayRectPlan BuildDisplayRectPlan(
    std::initializer_list<wallpaper::DisplayRect> rects) {
  wallpaper::DisplayRectPlan plan;
  for (const wallpaper::DisplayRect rect : rects) {
    if (!plan.PushBack(rect)) {
      break;
    }
  }
  return plan;
}

}  // namespace

TEST_CASE(MonitorLayoutPolicy_BuildsIndependentViewportsForSideBySideMonitors) {
  const wallpaper::DisplayRect virtualDesktop{-1920, 0, 1920, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{-1920, 0, 0, 1080},
      wallpaper::DisplayRect{0, 0, 1920, 1080},
  });

  const wallpaper::RenderViewportPlan viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.Size(), static_cast<std::size_t>(2));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[0].width, 1920);
  EXPECT_EQ(viewports[0].height, 1080);
  EXPECT_EQ(viewports[1].left, 1920);
  EXPECT_EQ(viewports[1].top, 0);
  EXPECT_EQ(viewports[1].width, 1920);
  EXPECT_EQ(viewports[1].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_BuildsViewportsForStackedMonitors) {
  const wallpaper::DisplayRect virtualDesktop{0, 0, 1920, 2160};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{0, 0, 1920, 1080},
      wallpaper::DisplayRect{0, 1080, 1920, 2160},
  });

  const wallpaper::RenderViewportPlan viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.Size(), static_cast<std::size_t>(2));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[1].left, 0);
  EXPECT_EQ(viewports[1].top, 1080);
  EXPECT_EQ(viewports[1].width, 1920);
  EXPECT_EQ(viewports[1].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_ClipsMonitorRectOutsideVirtualDesktop) {
  const wallpaper::DisplayRect virtualDesktop{0, 0, 1920, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{-120, -20, 1000, 1100},
  });

  const wallpaper::RenderViewportPlan viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.Size(), static_cast<std::size_t>(1));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[0].width, 1000);
  EXPECT_EQ(viewports[0].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_InvalidOrEmptyInputReturnsNoViewport) {
  const wallpaper::DisplayRect invalidVirtual{0, 0, 0, 0};
  const wallpaper::DisplayRectPlan noMonitors;

  const auto fromEmpty = wallpaper::BuildRenderMonitorViewports(
      wallpaper::DisplayRect{0, 0, 1920, 1080}, noMonitors);
  const auto fromInvalid = wallpaper::BuildRenderMonitorViewports(
      invalidVirtual, BuildDisplayRectPlan({wallpaper::DisplayRect{0, 0, 1920, 1080}}));

  EXPECT_TRUE(fromEmpty.Empty());
  EXPECT_TRUE(fromInvalid.Empty());
}

TEST_CASE(MonitorLayoutPolicy_RepeatedFrameUsesLargestMonitorSizeForMultiDisplayLayouts) {
  const wallpaper::DisplayRect virtualDesktop{-1920, 0, 1920, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{-1920, 0, 0, 1080},
      wallpaper::DisplayRect{0, 0, 1920, 1080},
  });

  const wallpaper::DisplaySize repeatedFrameSize =
      wallpaper::SelectRepeatedFrameRenderSize(virtualDesktop, monitors);

  EXPECT_EQ(repeatedFrameSize.width, 1920);
  EXPECT_EQ(repeatedFrameSize.height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_RepeatedFrameKeepsVirtualDesktopSizeForSingleDisplay) {
  const wallpaper::DisplayRect virtualDesktop{0, 0, 3840, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{0, 0, 3840, 1080},
  });

  const wallpaper::DisplaySize repeatedFrameSize =
      wallpaper::SelectRepeatedFrameRenderSize(virtualDesktop, monitors);

  EXPECT_EQ(repeatedFrameSize.width, 3840);
  EXPECT_EQ(repeatedFrameSize.height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_ScalesViewportsToCurrentRenderTargetSize) {
  const wallpaper::DisplayRect virtualDesktop{-1920, 0, 1920, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{-1920, 0, 0, 1080},
      wallpaper::DisplayRect{0, 0, 1920, 1080},
  });

  const wallpaper::RenderViewportPlan viewports =
      wallpaper::BuildScaledRenderMonitorViewports(virtualDesktop, monitors, 1920, 1080);

  EXPECT_EQ(viewports.Size(), static_cast<std::size_t>(2));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[0].width, 960);
  EXPECT_EQ(viewports[0].height, 1080);
  EXPECT_EQ(viewports[1].left, 960);
  EXPECT_EQ(viewports[1].top, 0);
  EXPECT_EQ(viewports[1].width, 960);
  EXPECT_EQ(viewports[1].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_ScaledViewportFallbacksToSingleViewForInvalidRenderTarget) {
  const wallpaper::DisplayRect virtualDesktop{0, 0, 1920, 1080};
  const wallpaper::DisplayRectPlan monitors = BuildDisplayRectPlan({
      wallpaper::DisplayRect{0, 0, 1920, 1080},
  });

  const wallpaper::RenderViewportPlan viewports =
      wallpaper::BuildScaledRenderMonitorViewports(virtualDesktop, monitors, 0, 1080);

  EXPECT_TRUE(viewports.Empty());
}

TEST_CASE(MonitorLayoutPolicy_DisplayRectPlanStopsGrowingAtFixedCapacity) {
  wallpaper::DisplayRectPlan plan;

  for (std::size_t index = 0; index < plan.Capacity(); ++index) {
    EXPECT_TRUE(plan.PushBack(wallpaper::DisplayRect{
        static_cast<int>(index), 0, static_cast<int>(index) + 1, 1}));
  }

  EXPECT_EQ(plan.Size(), plan.Capacity());
  EXPECT_FALSE(plan.PushBack(wallpaper::DisplayRect{999, 0, 1000, 1}));
  EXPECT_EQ(plan.Size(), plan.Capacity());
  EXPECT_EQ(plan[plan.Size() - 1].left, static_cast<int>(plan.Capacity() - 1));
}
