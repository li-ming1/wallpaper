#include "wallpaper/app.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstring>
#include <filesystem>

namespace {

void EnablePerMonitorDpiAwareness() {
#ifdef _WIN32
  using SetDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
  const HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32 == nullptr) {
    return;
  }

  const FARPROC rawProc = GetProcAddress(user32, "SetProcessDpiAwarenessContext");
  SetDpiAwarenessContextFn setDpiAwarenessContext = nullptr;
  static_assert(sizeof(setDpiAwarenessContext) == sizeof(rawProc));
  std::memcpy(&setDpiAwarenessContext, &rawProc, sizeof(setDpiAwarenessContext));
  if (setDpiAwarenessContext != nullptr) {
    const HANDLE kPerMonitorV2 = reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-4));
    setDpiAwarenessContext(kPerMonitorV2);
    return;
  }
  SetProcessDPIAware();
#endif
}

std::filesystem::path ResolveConfigPath() {
#ifdef _WIN32
  wchar_t exePath[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return std::filesystem::path{"config.json"};
  }

  std::filesystem::path base(exePath);
  const auto parent = base.parent_path();
  if (parent.empty()) {
    return std::filesystem::path{"config.json"};
  }
  return parent / "config.json";
#else
  return std::filesystem::path{"config.json"};
#endif
}

int RunWallpaperApp() {
  EnablePerMonitorDpiAwareness();
  wallpaper::App app(ResolveConfigPath());
  if (!app.Initialize()) {
    return 1;
  }
  return app.Run();
}

}  // namespace

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  return RunWallpaperApp();
}

int main() {
  return RunWallpaperApp();
}
#else
int main() {
  return RunWallpaperApp();
}
#endif
