#include "wallpaper/interfaces.h"

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <cmath>
#include <cstdint>
#include <iterator>
#endif

namespace wallpaper {

namespace {

#ifdef _WIN32

template <typename T>
void SafeRelease(T** ptr) {
  if (ptr != nullptr && *ptr != nullptr) {
    (*ptr)->Release();
    *ptr = nullptr;
  }
}

constexpr wchar_t kWallpaperWindowClassName[] = L"WallpaperRenderHostWindow";

LRESULT CALLBACK WallpaperWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_NCHITTEST:
      // 壁纸层不应截获鼠标，保持桌面图标与系统交互可用。
      return HTTRANSPARENT;
    default:
      return DefWindowProcW(hwnd, message, wParam, lParam);
  }
}

bool RegisterWallpaperWindowClass(HINSTANCE instance) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = WallpaperWndProc;
  wc.hInstance = instance;
  wc.lpszClassName = kWallpaperWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

RECT GetVirtualScreenRect() {
  RECT rect{};
  rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  return rect;
}

HWND FindWorkerWWindow() {
  HWND progman = FindWindowW(L"Progman", nullptr);
  if (progman != nullptr) {
    DWORD_PTR result = 0;
    // 向 Progman 发送 0x052C 触发 WorkerW，兼容多版本桌面 shell。
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
  }

  HWND workerw = nullptr;
  HWND top = nullptr;
  while ((top = FindWindowExW(nullptr, top, L"WorkerW", nullptr)) != nullptr) {
    HWND defView = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr) {
      workerw = top;
      break;
    }
  }
  if (workerw != nullptr) {
    return workerw;
  }

  // 回退路径：枚举任意顶层窗口，寻找包含 SHELLDLL_DefView 的宿主，再取其相邻 WorkerW。
  HWND enumerator = nullptr;
  while ((enumerator = FindWindowExW(nullptr, enumerator, nullptr, nullptr)) != nullptr) {
    HWND defView = FindWindowExW(enumerator, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView == nullptr) {
      continue;
    }
    workerw = FindWindowExW(nullptr, enumerator, L"WorkerW", nullptr);
    if (workerw != nullptr) {
      return workerw;
    }
  }

  return nullptr;
}

class WallpaperHostWin final : public IWallpaperHost {
 public:
  WallpaperHostWin() = default;

  ~WallpaperHostWin() override { DetachFromDesktop(); }

  bool AttachToDesktop() override {
    if (attached_) {
      return true;
    }

    instance_ = GetModuleHandleW(nullptr);
    if (instance_ == nullptr || !RegisterWallpaperWindowClass(instance_)) {
      return false;
    }

    desktopParent_ = FindWorkerWWindow();
    if (desktopParent_ == nullptr) {
      desktopParent_ = GetDesktopWindow();
    }
    if (desktopParent_ == nullptr) {
      return false;
    }

    // 使用虚拟桌面边界，保证多显示器（含负坐标副屏）能被统一覆盖。
    RECT bounds = GetVirtualScreenRect();
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
      // 回退：虚拟桌面指标异常时，使用宿主窗口客户区避免初始化失败。
      if (!GetClientRect(desktopParent_, &bounds)) {
        return false;
      }
    }

    const DWORD style = WS_CHILD | WS_VISIBLE;
    renderWindow_ = CreateWindowExW(
        WS_EX_NOACTIVATE, kWallpaperWindowClassName, L"", style, bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top, desktopParent_, nullptr,
        instance_, nullptr);
    if (renderWindow_ == nullptr) {
      return false;
    }

    if (!InitializeD3D()) {
      DestroyWindow(renderWindow_);
      renderWindow_ = nullptr;
      return false;
    }

    attached_ = true;
    return true;
  }

  void DetachFromDesktop() override {
    attached_ = false;
    ReleaseD3D();
    if (renderWindow_ != nullptr) {
      DestroyWindow(renderWindow_);
      renderWindow_ = nullptr;
    }
    desktopParent_ = nullptr;
  }

  void ResizeForDisplays() override {
    if (renderWindow_ == nullptr) {
      return;
    }

    const RECT virtualRect = GetVirtualScreenRect();
    const int width = virtualRect.right - virtualRect.left;
    const int height = virtualRect.bottom - virtualRect.top;
    if (width <= 0 || height <= 0) {
      return;
    }

    SetWindowPos(renderWindow_, HWND_BOTTOM, virtualRect.left, virtualRect.top, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ResizeSwapChain(static_cast<UINT>(width), static_cast<UINT>(height));
  }

  void Present(const FrameToken& frame) override {
    if (context_ == nullptr || renderTargetView_ == nullptr || swapChain_ == nullptr) {
      return;
    }

    const float phase = static_cast<float>(frame.sequence % 6000ULL) * 0.0020f;
    // 使用低频正弦变化产生平滑动态色彩，确保即使无视频输入也有可见动态效果。
    const float red = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase));
    const float green = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase + 2.10f));
    const float blue = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase + 4.20f));
    const float color[4] = {red, green, blue, 1.0f};

    context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->ClearRenderTargetView(renderTargetView_, color);
    swapChain_->Present(1, 0);
  }

 private:
  bool InitializeD3D() {
    UINT createFlags = 0;
#ifndef NDEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    const UINT levelCount = static_cast<UINT>(sizeof(levels) / sizeof(levels[0]));

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                                   levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
    if (FAILED(hr) && (createFlags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
      createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                             levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
    }
    if (FAILED(hr)) {
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, levels,
                             levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
      if (FAILED(hr)) {
        return false;
      }
    }

    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;
    hr = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
      return false;
    }
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
      SafeRelease(&dxgiDevice);
      return false;
    }
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory));
    SafeRelease(&adapter);
    SafeRelease(&dxgiDevice);
    if (FAILED(hr)) {
      return false;
    }

    RECT rect{};
    GetClientRect(renderWindow_, &rect);
    const UINT width = static_cast<UINT>(rect.right - rect.left);
    const UINT height = static_cast<UINT>(rect.bottom - rect.top);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Scaling = DXGI_SCALING_STRETCH;

    hr = factory->CreateSwapChainForHwnd(device_, renderWindow_, &desc, nullptr, nullptr, &swapChain_);
    SafeRelease(&factory);
    if (FAILED(hr)) {
      return false;
    }

    return CreateRenderTargetView();
  }

  bool CreateRenderTargetView() {
    SafeRelease(&renderTargetView_);
    ID3D11Texture2D* buffer = nullptr;
    HRESULT hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                       reinterpret_cast<void**>(&buffer));
    if (FAILED(hr)) {
      return false;
    }

    hr = device_->CreateRenderTargetView(buffer, nullptr, &renderTargetView_);
    SafeRelease(&buffer);
    if (FAILED(hr)) {
      return false;
    }
    return true;
  }

  void ResizeSwapChain(UINT width, UINT height) {
    if (swapChain_ == nullptr || context_ == nullptr) {
      return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    SafeRelease(&renderTargetView_);
    if (SUCCEEDED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
      CreateRenderTargetView();
    }
  }

  void ReleaseD3D() {
    SafeRelease(&renderTargetView_);
    SafeRelease(&swapChain_);
    SafeRelease(&context_);
    SafeRelease(&device_);
  }

  HINSTANCE instance_ = nullptr;
  HWND desktopParent_ = nullptr;
  HWND renderWindow_ = nullptr;
  bool attached_ = false;

  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  IDXGISwapChain1* swapChain_ = nullptr;
  ID3D11RenderTargetView* renderTargetView_ = nullptr;
};

#else

class WallpaperHostWin final : public IWallpaperHost {
 public:
  bool AttachToDesktop() override { return false; }
  void DetachFromDesktop() override {}
  void ResizeForDisplays() override {}
  void Present(const FrameToken&) override {}
};

#endif

}  // namespace

std::unique_ptr<IWallpaperHost> CreateWallpaperHost() {
  return std::make_unique<WallpaperHostWin>();
}

}  // namespace wallpaper
