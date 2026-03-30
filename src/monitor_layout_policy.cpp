#include "wallpaper/monitor_layout_policy.h"

#include <algorithm>

namespace wallpaper {

namespace {

[[nodiscard]] bool IsValidRect(const DisplayRect& rect) noexcept {
  return rect.right > rect.left && rect.bottom > rect.top;
}

[[nodiscard]] DisplayRect ClipRect(const DisplayRect& rect, const DisplayRect& clip) noexcept {
  return DisplayRect{
      std::max(rect.left, clip.left),
      std::max(rect.top, clip.top),
      std::min(rect.right, clip.right),
      std::min(rect.bottom, clip.bottom),
  };
}

}  // namespace

std::vector<RenderViewport> BuildRenderMonitorViewports(
    const DisplayRect& virtualDesktop, const std::vector<DisplayRect>& monitors) noexcept {
  std::vector<RenderViewport> viewports;
  if (!IsValidRect(virtualDesktop) || monitors.empty()) {
    return viewports;
  }

  viewports.reserve(monitors.size());
  for (const DisplayRect& monitor : monitors) {
    if (!IsValidRect(monitor)) {
      continue;
    }

    // 先与虚拟桌面求交，再转成相对坐标，避免负坐标副屏导致视口越界。
    const DisplayRect clipped = ClipRect(monitor, virtualDesktop);
    if (!IsValidRect(clipped)) {
      continue;
    }

    const int width = clipped.right - clipped.left;
    const int height = clipped.bottom - clipped.top;
    if (width <= 0 || height <= 0) {
      continue;
    }

    viewports.push_back(RenderViewport{
        clipped.left - virtualDesktop.left,
        clipped.top - virtualDesktop.top,
        width,
        height,
    });
  }
  return viewports;
}

}  // namespace wallpaper
