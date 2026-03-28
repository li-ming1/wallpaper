#include "wallpaper/interfaces.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace wallpaper {
namespace {

#ifdef _WIN32

constexpr wchar_t kTrayWindowClassName[] = L"WallpaperTrayMessageWindow";
constexpr UINT kTrayIconMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kAppIconResourceId = 1;
constexpr UINT_PTR kMenuSet30FpsId = 1001;
constexpr UINT_PTR kMenuSet60FpsId = 1002;
constexpr UINT_PTR kMenuSelectVideoId = 1003;
constexpr UINT_PTR kMenuClearVideoId = 1004;
constexpr UINT_PTR kMenuEnableAutoStartId = 1005;
constexpr UINT_PTR kMenuDisableAutoStartId = 1006;
constexpr UINT_PTR kMenuEnableAdaptiveQualityId = 1007;
constexpr UINT_PTR kMenuDisableAdaptiveQualityId = 1008;
constexpr UINT_PTR kMenuExitId = 1099;

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                        static_cast<int>(value.size()), nullptr, 0, nullptr,
                                        nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string utf8(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), bytes,
                      nullptr, nullptr);
  return utf8;
}

class TrayControllerWin final : public ITrayController {
 public:
  ~TrayControllerWin() override { StopMessageLoop(); }

  void StartMessageLoop() override {
    if (running_.exchange(true)) {
      return;
    }
    exitRequested_.store(false);
    worker_ = std::thread([this]() { ThreadMain(); });
  }

  void StopMessageLoop() override {
    if (!running_.exchange(false)) {
      return;
    }

    const DWORD threadId = workerThreadId_.load();
    if (threadId != 0) {
      PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (worker_.joinable()) {
      worker_.join();
    }
    workerThreadId_.store(0);
  }

  void UpdateMenuState(const TrayMenuState& state) override {
    std::lock_guard<std::mutex> lock(menuStateMu_);
    menuState_ = state;
  }

  [[nodiscard]] bool IsExitRequested() const override {
    return exitRequested_.load();
  }

  bool TryDequeueAction(TrayAction* action) override {
    if (action == nullptr) {
      return false;
    }
    std::lock_guard<std::mutex> lock(actionsMu_);
    if (pendingActions_.empty()) {
      return false;
    }
    *action = std::move(pendingActions_.front());
    pendingActions_.pop_front();
    return true;
  }

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    TrayControllerWin* self = reinterpret_cast<TrayControllerWin*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
      self = reinterpret_cast<TrayControllerWin*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (self == nullptr) {
      return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
      case kTrayIconMessage:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU || lParam == WM_LBUTTONUP) {
          self->ShowContextMenu();
        }
        return 0;
      case WM_COMMAND:
        self->HandleMenuCommand(LOWORD(wParam));
        return 0;
        break;
      case WM_DESTROY:
        self->RemoveTrayIcon();
        return 0;
      default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  void ThreadMain() {
    workerThreadId_.store(GetCurrentThreadId());

    if (!RegisterWindowClass()) {
      running_.store(false);
      exitRequested_.store(true);
      return;
    }

    messageWindow_ = CreateWindowExW(0, kTrayWindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                     nullptr, GetModuleHandleW(nullptr), this);
    if (messageWindow_ == nullptr) {
      running_.store(false);
      exitRequested_.store(true);
      return;
    }

    if (!AddTrayIcon()) {
      DestroyWindow(messageWindow_);
      messageWindow_ = nullptr;
      running_.store(false);
      exitRequested_.store(true);
      return;
    }

    MSG msg{};
    while (running_.load() && GetMessageW(&msg, nullptr, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    RemoveTrayIcon();
    if (messageWindow_ != nullptr) {
      DestroyWindow(messageWindow_);
      messageWindow_ = nullptr;
    }
  }

  bool RegisterWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TrayControllerWin::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(kAppIconResourceId));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = kTrayWindowClassName;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  void PushAction(TrayActionType type, std::string payload = {}) {
    std::lock_guard<std::mutex> lock(actionsMu_);
    pendingActions_.push_back(TrayAction{type, std::move(payload)});
  }

  std::wstring PickVideoFileFromDialog() {
    wchar_t fileBuffer[MAX_PATH] = {};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = messageWindow_;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.mov;*.avi;*.wmv\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (!GetOpenFileNameW(&ofn)) {
      return {};
    }
    return std::wstring(fileBuffer);
  }

  void HandleMenuCommand(const UINT commandId) {
    switch (commandId) {
      case kMenuSet30FpsId:
        PushAction(TrayActionType::kSetFps30);
        break;
      case kMenuSet60FpsId:
        PushAction(TrayActionType::kSetFps60);
        break;
      case kMenuSelectVideoId: {
        const std::wstring selected = PickVideoFileFromDialog();
        if (!selected.empty()) {
          PushAction(TrayActionType::kSelectVideo, WideToUtf8(selected));
        }
        break;
      }
      case kMenuClearVideoId:
        PushAction(TrayActionType::kClearVideo);
        break;
      case kMenuEnableAutoStartId:
        PushAction(TrayActionType::kEnableAutoStart);
        break;
      case kMenuDisableAutoStartId:
        PushAction(TrayActionType::kDisableAutoStart);
        break;
      case kMenuEnableAdaptiveQualityId:
        PushAction(TrayActionType::kEnableAdaptiveQuality);
        break;
      case kMenuDisableAdaptiveQualityId:
        PushAction(TrayActionType::kDisableAdaptiveQuality);
        break;
      case kMenuExitId:
        PushAction(TrayActionType::kExit);
        exitRequested_.store(true);
        running_.store(false);
        PostQuitMessage(0);
        break;
      default:
        break;
    }
  }

  bool AddTrayIcon() {
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = messageWindow_;
    nid_.uID = kTrayIconId;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = kTrayIconMessage;
    nid_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(kAppIconResourceId));
    if (nid_.hIcon == nullptr) {
      nid_.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    }
    lstrcpynW(nid_.szTip, L"Wallpaper", 128);
    return Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
  }

  void RemoveTrayIcon() {
    if (nid_.hWnd != nullptr) {
      Shell_NotifyIconW(NIM_DELETE, &nid_);
      nid_.hWnd = nullptr;
    }
  }

  void ShowContextMenu() {
    if (messageWindow_ == nullptr) {
      return;
    }
    TrayMenuState state{};
    {
      std::lock_guard<std::mutex> lock(menuStateMu_);
      state = menuState_;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
      return;
    }
    const auto fps30Flags = static_cast<UINT>(MF_STRING |
                                              (state.fpsCap == 30 ? MF_CHECKED : MF_UNCHECKED) |
                                              (state.fpsCap == 30 ? MF_GRAYED : 0));
    const auto fps60Flags = static_cast<UINT>(MF_STRING |
                                              (state.fpsCap == 60 ? MF_CHECKED : MF_UNCHECKED) |
                                              (state.fpsCap == 60 ? MF_GRAYED : 0));
    AppendMenuW(menu, fps30Flags, kMenuSet30FpsId, L"Set 30 FPS");
    AppendMenuW(menu, fps60Flags, kMenuSet60FpsId, L"Set 60 FPS");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSelectVideoId, L"Select Video...");
    const auto clearVideoFlags = static_cast<UINT>(MF_STRING | (state.hasVideo ? 0 : MF_GRAYED));
    AppendMenuW(menu, clearVideoFlags, kMenuClearVideoId, L"Clear Video");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const auto enableAutoFlags = static_cast<UINT>(MF_STRING |
                                                   (state.autoStart ? MF_CHECKED : MF_UNCHECKED) |
                                                   (state.autoStart ? MF_GRAYED : 0));
    const auto disableAutoFlags =
        static_cast<UINT>(MF_STRING |
                          (!state.autoStart ? MF_CHECKED : MF_UNCHECKED) |
                          (!state.autoStart ? MF_GRAYED : 0));
    AppendMenuW(menu, enableAutoFlags, kMenuEnableAutoStartId, L"Enable Auto Start");
    AppendMenuW(menu, disableAutoFlags, kMenuDisableAutoStartId, L"Disable Auto Start");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const auto enableAdaptiveFlags =
        static_cast<UINT>(MF_STRING |
                          (state.adaptiveQuality ? MF_CHECKED : MF_UNCHECKED) |
                          (state.adaptiveQuality ? MF_GRAYED : 0));
    const auto disableAdaptiveFlags =
        static_cast<UINT>(MF_STRING |
                          (!state.adaptiveQuality ? MF_CHECKED : MF_UNCHECKED) |
                          (!state.adaptiveQuality ? MF_GRAYED : 0));
    AppendMenuW(menu, enableAdaptiveFlags, kMenuEnableAdaptiveQualityId,
                L"Enable Adaptive Quality");
    AppendMenuW(menu, disableAdaptiveFlags, kMenuDisableAdaptiveQualityId,
                L"Disable Adaptive Quality");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExitId, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(messageWindow_);
    PushAction(TrayActionType::kMenuOpened);
    const UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0,
                                        messageWindow_, nullptr);
    PushAction(TrayActionType::kMenuClosed);
    if (command != 0) {
      HandleMenuCommand(command);
    }
    PostMessageW(messageWindow_, WM_NULL, 0, 0);

    DestroyMenu(menu);
  }

  std::atomic<bool> running_{false};
  std::atomic<bool> exitRequested_{false};
  std::thread worker_;
  std::atomic<DWORD> workerThreadId_{0};
  HWND messageWindow_ = nullptr;
  NOTIFYICONDATAW nid_{};
  std::mutex actionsMu_;
  std::deque<TrayAction> pendingActions_;
  std::mutex menuStateMu_;
  TrayMenuState menuState_{};
};

#else

class TrayControllerWin final : public ITrayController {
 public:
  void StartMessageLoop() override {}
  void StopMessageLoop() override {}
  void UpdateMenuState(const TrayMenuState&) override {}
  [[nodiscard]] bool IsExitRequested() const override { return false; }
  bool TryDequeueAction(TrayAction*) override { return false; }
};

#endif

}  // namespace

std::unique_ptr<ITrayController> CreateTrayController() {
  return std::make_unique<TrayControllerWin>();
}

}  // namespace wallpaper
