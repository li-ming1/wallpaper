#include "app_autostart.h"

#ifdef _WIN32
#include <windows.h>
#include <string>

namespace wallpaper {

bool SetAutoStartEnabled(const bool enabled) {
  constexpr wchar_t kRunPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  constexpr wchar_t kValueName[] = L"WallpaperDynamicDesktop";

  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return false;
  }

  bool ok = false;
  if (enabled) {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
      const std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
      const BYTE* bytes = reinterpret_cast<const BYTE*>(cmd.c_str());
      const DWORD byteSize = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
      ok = RegSetValueExW(key, kValueName, 0, REG_SZ, bytes, byteSize) == ERROR_SUCCESS;
    }
  } else {
    const LONG rc = RegDeleteValueW(key, kValueName);
    ok = (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
  }

  RegCloseKey(key);
  return ok;
}

}  // namespace wallpaper
#else

namespace wallpaper {
bool SetAutoStartEnabled(bool) { return true; }
}  // namespace wallpaper

#endif
