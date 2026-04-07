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

[[nodiscard]] DisplaySize ToDisplaySize(const DisplayRect& rect) noexcept {
  if (!IsValidRect(rect)) {
    return {};
  }
  return DisplaySize{
      rect.right - rect.left,
      rect.bottom - rect.top,
  };
}

[[nodiscard]] int ScaleAxisToRenderTarget(const int offset, const int sourceExtent,
                                          const int targetExtent) noexcept {
  if (sourceExtent <= 0 || targetExtent <= 0) {
    return 0;
  }
  return static_cast<int>((static_cast<long long>(offset) * targetExtent) / sourceExtent);
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

std::vector<RenderViewport> BuildScaledRenderMonitorViewports(
    const DisplayRect& virtualDesktop, const std::vector<DisplayRect>& monitors,
    const int renderTargetWidth, const int renderTargetHeight) noexcept {
  std::vector<RenderViewport> scaledViewports;
  if (renderTargetWidth <= 0 || renderTargetHeight <= 0) {
    return scaledViewports;
  }

  const std::vector<RenderViewport> baseViewports =
      BuildRenderMonitorViewports(virtualDesktop, monitors);
  if (baseViewports.empty()) {
    return scaledViewports;
  }

  const DisplaySize virtualSize = ToDisplaySize(virtualDesktop);
  if (virtualSize.width <= 0 || virtualSize.height <= 0) {
    return scaledViewports;
  }

  scaledViewports.reserve(baseViewports.size());
  for (const RenderViewport& viewport : baseViewports) {
    if (viewport.width <= 0 || viewport.height <= 0) {
      continue;
    }

    const int left = ScaleAxisToRenderTarget(viewport.left, virtualSize.width, renderTargetWidth);
    const int top = ScaleAxisToRenderTarget(viewport.top, virtualSize.height, renderTargetHeight);
    const int right =
        ScaleAxisToRenderTarget(viewport.left + viewport.width, virtualSize.width, renderTargetWidth);
    const int bottom = ScaleAxisToRenderTarget(viewport.top + viewport.height, virtualSize.height,
                                               renderTargetHeight);
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0) {
      continue;
    }

    scaledViewports.push_back(RenderViewport{
        left,
        top,
        width,
        height,
    });
  }
  return scaledViewports;
}

DisplaySize SelectRepeatedFrameRenderSize(const DisplayRect& virtualDesktop,
                                          const std::vector<DisplayRect>& monitors) noexcept {
  const DisplaySize virtualSize = ToDisplaySize(virtualDesktop);
  if (virtualSize.width <= 0 || virtualSize.height <= 0) {
    return {};
  }

  DisplaySize largestMonitorSize{};
  int validMonitorCount = 0;
  long long largestMonitorArea = 0;
  for (const DisplayRect& monitor : monitors) {
    if (!IsValidRect(monitor)) {
      continue;
    }

    const DisplayRect clipped = ClipRect(monitor, virtualDesktop);
    const DisplaySize clippedSize = ToDisplaySize(clipped);
    if (clippedSize.width <= 0 || clippedSize.height <= 0) {
      continue;
    }

    ++validMonitorCount;
    const long long area = static_cast<long long>(clippedSize.width) * clippedSize.height;
    if (area > largestMonitorArea) {
      largestMonitorArea = area;
      largestMonitorSize = clippedSize;
    }
  }

  // 当前渲染链路会把“同一帧”重复绘制到每个显示器视口。
  // 多显示器时应按单块显示器的最大尺寸请求解码输出，避免把整块虚拟桌面协商成超宽/超高帧。
  if (validMonitorCount > 1 && largestMonitorSize.width > 0 && largestMonitorSize.height > 0) {
    return largestMonitorSize;
  }
  return virtualSize;
}

}  // namespace wallpaper
