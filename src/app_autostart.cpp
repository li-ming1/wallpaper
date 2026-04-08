#include "app_autostart.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace wallpaper {
namespace {

std::wstring QueryCurrentModulePath() {
  constexpr std::size_t kInitialPathCapacity = 512;
  constexpr std::size_t kMaxPathCapacity = 32768;

  std::vector<wchar_t> buffer(kInitialPathCapacity, L'\0');
  for (;;) {
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0) {
      return {};
    }
    if (len < buffer.size()) {
      return std::wstring(buffer.data(), len);
    }
    if (buffer.size() >= kMaxPathCapacity) {
      return {};
    }
    buffer.resize(std::min(buffer.size() * 2, kMaxPathCapacity));
  }
}

}  // namespace

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
    const std::wstring exePath = QueryCurrentModulePath();
    if (!exePath.empty()) {
      const std::wstring cmd = L"\"" + exePath + L"\"";
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
