#include "wallpaper/interfaces.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <atomic>
#include <thread>

namespace wallpaper {
namespace {

#ifdef _WIN32

constexpr wchar_t kTrayWindowClassName[] = L"WallpaperTrayMessageWindow";
constexpr UINT kTrayIconMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kMenuExitId = 1001;

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

  [[nodiscard]] bool IsExitRequested() const override {
    return exitRequested_.load();
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
        if (LOWORD(wParam) == kMenuExitId) {
          self->exitRequested_.store(true);
          self->running_.store(false);
          PostQuitMessage(0);
          return 0;
        }
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
    wc.lpszClassName = kTrayWindowClassName;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  bool AddTrayIcon() {
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = messageWindow_;
    nid_.uID = kTrayIconId;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = kTrayIconMessage;
    nid_.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
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

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
      return;
    }
    AppendMenuW(menu, MF_STRING, kMenuExitId, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(messageWindow_);
    const UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0,
                                        messageWindow_, nullptr);
    if (command == kMenuExitId) {
      exitRequested_.store(true);
      running_.store(false);
      PostQuitMessage(0);
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
};

#else

class TrayControllerWin final : public ITrayController {
 public:
  void StartMessageLoop() override {}
  void StopMessageLoop() override {}
  [[nodiscard]] bool IsExitRequested() const override { return false; }
};

#endif

}  // namespace

std::unique_ptr<ITrayController> CreateTrayController() {
  return std::make_unique<TrayControllerWin>();
}

}  // namespace wallpaper
