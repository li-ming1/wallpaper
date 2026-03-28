#include "wallpaper/desktop_context_policy.h"

#include <algorithm>
#include <array>
#include <cwctype>

namespace wallpaper {
namespace {

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
  return value;
}

}  // namespace

bool IsDesktopContextClass(const std::wstring& className) {
  if (className.empty()) {
    return false;
  }

  static constexpr std::array<const wchar_t*, 8> kDesktopClasses = {
      L"Progman",
      L"WorkerW",
      L"SHELLDLL_DefView",
      L"SysListView32",
      L"WallpaperRenderHostWindow",
      L"WallpaperTrayMessageWindow",
      L"Shell_TrayWnd",
      L"CabinetWClass",
  };

  return std::find_if(kDesktopClasses.begin(), kDesktopClasses.end(),
                      [&](const wchar_t* expected) { return className == expected; }) !=
         kDesktopClasses.end();
}

bool IsExplorerProcess(const std::wstring& processName) {
  if (processName.empty()) {
    return false;
  }
  return ToLower(processName) == L"explorer.exe";
}

bool ShouldTreatAsDesktopContext(const std::wstring& className,
                                 const std::wstring& processName) {
  if (IsDesktopContextClass(className)) {
    return true;
  }
  return IsExplorerProcess(processName);
}

}  // namespace wallpaper
