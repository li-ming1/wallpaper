#pragma once

#include <vector>

namespace wallpaper {

struct DisplayRect final {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

struct RenderViewport final {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
};

[[nodiscard]] std::vector<RenderViewport> BuildRenderMonitorViewports(
    const DisplayRect& virtualDesktop, const std::vector<DisplayRect>& monitors) noexcept;

}  // namespace wallpaper
