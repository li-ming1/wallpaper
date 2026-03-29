#include "wallpaper/app.h"
#include "wallpaper/single_instance_policy.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

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

#ifdef _WIN32
class ScopedSingleInstanceMutex final {
 public:
  ScopedSingleInstanceMutex() = default;
  ~ScopedSingleInstanceMutex() {
    ReleaseLockFileHandle();
    ReleaseMutexHandle();
  }

  [[nodiscard]] bool TryAcquire() {
    mutexAcquired_ = TryAcquireNamedMutexWithFallback();
    if (!mutexAcquired_) {
      lockFileAcquired_ = false;
      return false;
    }

    lockFileAcquired_ = TryAcquireLockFile();
    if (!lockFileAcquired_) {
      ReleaseMutexHandle();
      return false;
    }

    return true;
  }

  [[nodiscard]] bool mutex_acquired() const noexcept { return mutexAcquired_; }
  [[nodiscard]] bool lock_file_acquired() const noexcept { return lockFileAcquired_; }

 private:
  [[nodiscard]] bool TryAcquireNamedMutexWithFallback() {
    DWORD globalError = 0;
    if (TryAcquireNamedMutex(L"Global\\WallpaperDynamicDesktop.Singleton", &globalError)) {
      return true;
    }
    // Only fallback when global namespace is unavailable; if it already exists, second instance must exit.
    if (!wallpaper::ShouldFallbackToLocalMutex(globalError)) {
      return false;
    }

    DWORD localError = 0;
    return TryAcquireNamedMutex(L"Local\\WallpaperDynamicDesktop.Singleton", &localError);
  }

  [[nodiscard]] bool TryAcquireNamedMutex(const wchar_t* name, DWORD* errorCode) {
    if (name == nullptr || *name == L'\0') {
      if (errorCode != nullptr) {
        *errorCode = ERROR_INVALID_PARAMETER;
      }
      return false;
    }
    HANDLE candidate = CreateMutexW(nullptr, FALSE, name);
    if (candidate == nullptr) {
      if (errorCode != nullptr) {
        *errorCode = GetLastError();
      }
      return false;
    }
    const DWORD err = GetLastError();
    if (errorCode != nullptr) {
      *errorCode = err;
    }
    if (wallpaper::IsExistingInstanceErrorCode(err)) {
      CloseHandle(candidate);
      return false;
    }
    handle_ = candidate;
    return true;
  }

  static std::filesystem::path ResolveLockFilePath() {
    wchar_t tempPath[MAX_PATH] = {};
    const DWORD len = GetTempPathW(MAX_PATH, tempPath);
    if (len == 0 || len >= MAX_PATH) {
      return {};
    }
    return std::filesystem::path(tempPath) / "wallpaper" / "instance.lock";
  }

  [[nodiscard]] bool TryAcquireLockFile() {
    const std::filesystem::path lockPath = ResolveLockFilePath();
    if (lockPath.empty()) {
      return false;
    }
    std::error_code createDirError;
    std::filesystem::create_directories(lockPath.parent_path(), createDirError);
    HANDLE candidate =
        CreateFileW(lockPath.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (candidate == INVALID_HANDLE_VALUE) {
      return false;
    }
    fileHandle_ = candidate;
    return true;
  }

  void ReleaseMutexHandle() {
    if (handle_ != nullptr) {
      CloseHandle(handle_);
      handle_ = nullptr;
    }
  }

  void ReleaseLockFileHandle() {
    if (fileHandle_ != nullptr) {
      CloseHandle(fileHandle_);
      fileHandle_ = nullptr;
    }
  }

  HANDLE handle_ = nullptr;
  HANDLE fileHandle_ = nullptr;
  bool mutexAcquired_ = false;
  bool lockFileAcquired_ = false;
};
#endif

int RunWallpaperApp() {
#ifdef _WIN32
  ScopedSingleInstanceMutex singleInstanceMutex;
  const bool guardAcquired = singleInstanceMutex.TryAcquire();
  if (!wallpaper::ShouldAllowSingleInstanceStartup(guardAcquired && singleInstanceMutex.mutex_acquired(),
                                                   singleInstanceMutex.lock_file_acquired(), false)) {
    return 0;
  }
#endif
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
