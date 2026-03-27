#include "wallpaper/app.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <filesystem>

namespace {

std::filesystem::path ResolveConfigPath() {
#ifdef _WIN32
  wchar_t localAppData[MAX_PATH] = {};
  const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT,
                                      localAppData);
  if (FAILED(hr) || localAppData[0] == L'\0') {
    return std::filesystem::path{"config.json"};
  }

  std::filesystem::path base(localAppData);
  return base / "wallpaper" / "config.json";
#else
  return std::filesystem::path{"config.json"};
#endif
}

int RunWallpaperApp() {
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
