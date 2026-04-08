#include "wallpaper/app.h"
#include "wallpaper/single_instance_policy.h"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

void EnablePerMonitorDpiAwareness() {
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
}

std::filesystem::path ResolveConfigPath() {
  constexpr std::size_t kInitialPathCapacity = 512;
  constexpr std::size_t kMaxPathCapacity = 32768;

  std::vector<wchar_t> exePath(kInitialPathCapacity, L'\0');
  for (;;) {
    const DWORD len = GetModuleFileNameW(nullptr, exePath.data(),
                                         static_cast<DWORD>(exePath.size()));
    if (len == 0) {
      return std::filesystem::path{"config.json"};
    }
    if (len < exePath.size()) {
      std::filesystem::path base(std::wstring(exePath.data(), len));
      const auto parent = base.parent_path();
      if (parent.empty()) {
        return std::filesystem::path{"config.json"};
      }
      return parent / "config.json";
    }
    if (exePath.size() >= kMaxPathCapacity) {
      return std::filesystem::path{"config.json"};
    }
    exePath.resize(std::min(exePath.size() * 2, kMaxPathCapacity));
  }
}

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
    constexpr std::size_t kInitialTempPathCapacity = 512;
    constexpr std::size_t kMaxTempPathCapacity = 32768;

    std::vector<wchar_t> tempPath(kInitialTempPathCapacity, L'\0');
    for (;;) {
      const DWORD len = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
      if (len == 0) {
        return {};
      }
      if (len < tempPath.size()) {
        return std::filesystem::path(std::wstring(tempPath.data(), len)) / "wallpaper" /
               "instance.lock";
      }
      if (tempPath.size() >= kMaxTempPathCapacity) {
        return {};
      }
      const std::size_t expandedCapacity =
          std::max<std::size_t>(tempPath.size() * 2, static_cast<std::size_t>(len) + 1U);
      tempPath.resize(std::min(expandedCapacity, kMaxTempPathCapacity));
    }
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

class ScopedComInit final {
 public:
  ScopedComInit() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
      initialized_ = true;
      return;
    }
    if (hr == RPC_E_CHANGED_MODE) {
      // 若外部已以其他模型初始化 COM，不重复初始化，沿用当前线程模型。
      initialized_ = false;
      return;
    }
    initialized_ = false;
  }

  ~ScopedComInit() {
    if (initialized_) {
      CoUninitialize();
    }
  }

 private:
  bool initialized_ = false;
};

int RunWallpaperApp() {
  ScopedComInit comInit;
  ScopedSingleInstanceMutex singleInstanceMutex;
  const bool guardAcquired = singleInstanceMutex.TryAcquire();
  if (!wallpaper::ShouldAllowSingleInstanceStartup(guardAcquired && singleInstanceMutex.mutex_acquired(),
                                                   singleInstanceMutex.lock_file_acquired())) {
    return 0;
  }
  EnablePerMonitorDpiAwareness();
  wallpaper::App app(ResolveConfigPath());
  if (!app.Initialize()) {
    return 1;
  }
  return app.Run();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  return RunWallpaperApp();
}

int main() {
  return RunWallpaperApp();
}
