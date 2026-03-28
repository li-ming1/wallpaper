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
