#include "wallpaper/monitor_layout_policy.h"

#include <vector>

#include "test_support.h"

TEST_CASE(MonitorLayoutPolicy_BuildsIndependentViewportsForSideBySideMonitors) {
  const wallpaper::DisplayRect virtualDesktop{-1920, 0, 1920, 1080};
  const std::vector<wallpaper::DisplayRect> monitors = {
      wallpaper::DisplayRect{-1920, 0, 0, 1080},
      wallpaper::DisplayRect{0, 0, 1920, 1080},
  };

  const std::vector<wallpaper::RenderViewport> viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.size(), static_cast<std::size_t>(2));
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
  const std::vector<wallpaper::DisplayRect> monitors = {
      wallpaper::DisplayRect{0, 0, 1920, 1080},
      wallpaper::DisplayRect{0, 1080, 1920, 2160},
  };

  const std::vector<wallpaper::RenderViewport> viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[1].left, 0);
  EXPECT_EQ(viewports[1].top, 1080);
  EXPECT_EQ(viewports[1].width, 1920);
  EXPECT_EQ(viewports[1].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_ClipsMonitorRectOutsideVirtualDesktop) {
  const wallpaper::DisplayRect virtualDesktop{0, 0, 1920, 1080};
  const std::vector<wallpaper::DisplayRect> monitors = {
      wallpaper::DisplayRect{-120, -20, 1000, 1100},
  };

  const std::vector<wallpaper::RenderViewport> viewports =
      wallpaper::BuildRenderMonitorViewports(virtualDesktop, monitors);

  EXPECT_EQ(viewports.size(), static_cast<std::size_t>(1));
  EXPECT_EQ(viewports[0].left, 0);
  EXPECT_EQ(viewports[0].top, 0);
  EXPECT_EQ(viewports[0].width, 1000);
  EXPECT_EQ(viewports[0].height, 1080);
}

TEST_CASE(MonitorLayoutPolicy_InvalidOrEmptyInputReturnsNoViewport) {
  const wallpaper::DisplayRect invalidVirtual{0, 0, 0, 0};
  const std::vector<wallpaper::DisplayRect> noMonitors;

  const auto fromEmpty = wallpaper::BuildRenderMonitorViewports(
      wallpaper::DisplayRect{0, 0, 1920, 1080}, noMonitors);
  const auto fromInvalid = wallpaper::BuildRenderMonitorViewports(
      invalidVirtual, std::vector<wallpaper::DisplayRect>{wallpaper::DisplayRect{0, 0, 1920, 1080}});

  EXPECT_TRUE(fromEmpty.empty());
  EXPECT_TRUE(fromInvalid.empty());
}
