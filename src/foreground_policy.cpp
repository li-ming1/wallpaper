#include "wallpaper/foreground_policy.h"

#include <algorithm>
#include <array>

namespace wallpaper {

bool IsNearlyCoveringMonitor(const int windowLeft, const int windowTop, const int windowRight,
                             const int windowBottom, const int monitorLeft, const int monitorTop,
                             const int monitorRight, const int monitorBottom,
                             const int tolerancePx) noexcept {
  const int tolerance = std::max(tolerancePx, 0);
  return windowLeft <= monitorLeft + tolerance && windowTop <= monitorTop + tolerance &&
         windowRight >= monitorRight - tolerance &&
         windowBottom >= monitorBottom - tolerance;
}

double ComputeCoverageRatio(const int windowLeft, const int windowTop, const int windowRight,
                            const int windowBottom, const int monitorLeft, const int monitorTop,
                            const int monitorRight, const int monitorBottom) noexcept {
  const int overlapLeft = std::max(windowLeft, monitorLeft);
  const int overlapTop = std::max(windowTop, monitorTop);
  const int overlapRight = std::min(windowRight, monitorRight);
  const int overlapBottom = std::min(windowBottom, monitorBottom);
  const int overlapWidth = std::max(overlapRight - overlapLeft, 0);
  const int overlapHeight = std::max(overlapBottom - overlapTop, 0);
  const int monitorWidth = std::max(monitorRight - monitorLeft, 0);
  const int monitorHeight = std::max(monitorBottom - monitorTop, 0);
  if (monitorWidth == 0 || monitorHeight == 0) {
    return 0.0;
  }
  const double overlapArea = static_cast<double>(overlapWidth) * static_cast<double>(overlapHeight);
  const double monitorArea = static_cast<double>(monitorWidth) * static_cast<double>(monitorHeight);
  return overlapArea / monitorArea;
}

bool IsLikelyFullscreenWindow(const bool isBorderlessPopup,
                              const double coverageRatio) noexcept {
  if (coverageRatio >= 0.985) {
    return true;
  }
  if (isBorderlessPopup && coverageRatio >= 0.88) {
    return true;
  }
  return false;
}

bool IsShellForegroundClass(const std::wstring& className) {
  if (className.empty()) {
    return false;
  }

  static constexpr std::array<const wchar_t*, 6> kShellClasses = {
      L"Progman",
      L"WorkerW",
      L"SHELLDLL_DefView",
      L"SysListView32",
      L"WallpaperRenderHostWindow",
      L"Shell_TrayWnd",
  };

  return std::find_if(kShellClasses.begin(), kShellClasses.end(),
                      [&](const wchar_t* expected) { return className == expected; }) !=
         kShellClasses.end();
}

bool ShouldTreatForegroundAsFullscreen(const std::wstring& className, const bool coversMonitor,
                                       const bool isVisible) {
  if (!coversMonitor) {
    return false;
  }
  if (IsShellForegroundClass(className)) {
    return false;
  }
  (void)isVisible;
  return true;
}

}  // namespace wallpaper
