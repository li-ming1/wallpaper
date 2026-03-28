#include "wallpaper/foreground_policy.h"

#include <algorithm>
#include <array>

namespace wallpaper {

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
  if (!isVisible || !coversMonitor) {
    return false;
  }
  if (IsShellForegroundClass(className)) {
    return false;
  }
  return true;
}

}  // namespace wallpaper
