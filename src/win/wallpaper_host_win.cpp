#include "wallpaper/interfaces.h"
#include "wallpaper/frame_bridge.h"
#include "wallpaper/desktop_attach_policy.h"

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>
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

struct VideoVertex final {
  float x;
  float y;
  float u;
  float v;
};

constexpr char kVideoVsSource[] = R"(
struct VSIn {
  float2 pos : POSITION;
  float2 uv  : TEXCOORD0;
};

struct VSOut {
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
};

VSOut main(VSIn input) {
  VSOut outv;
  outv.pos = float4(input.pos, 0.0, 1.0);
  outv.uv = input.uv;
  return outv;
}
)";

constexpr char kVideoPsSource[] = R"(
Texture2D videoTex : register(t0);
SamplerState videoSamp : register(s0);

struct PSIn {
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET {
  return videoTex.Sample(videoSamp, input.uv);
}
)";

bool CompileShader(const char* source, const char* entryPoint, const char* profile,
                   ID3DBlob** outBlob) {
  if (source == nullptr || entryPoint == nullptr || profile == nullptr || outBlob == nullptr) {
    return false;
  }

  *outBlob = nullptr;
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

  ID3DBlob* errorBlob = nullptr;
  const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
                                entryPoint, profile, flags, 0, outBlob, &errorBlob);
  SafeRelease(&errorBlob);
  return SUCCEEDED(hr) && *outBlob != nullptr;
}

LRESULT CALLBACK WallpaperWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_NCHITTEST:
      // 壁纸层不应截获鼠标，保持桌面图标与系统交互可用。
      return HTTRANSPARENT;
    case WM_SETCURSOR:
      SetCursor(LoadCursorW(nullptr, IDC_ARROW));
      return TRUE;
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
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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

RECT GetVirtualChildBounds() {
  RECT bounds = GetVirtualScreenRect();
  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  if (width <= 0 || height <= 0) {
    return RECT{0, 0, 1280, 720};
  }
  return RECT{0, 0, width, height};
}

RECT GetParentBoundsOrVirtual(HWND parent) {
  RECT bounds{};
  if (parent != nullptr && GetClientRect(parent, &bounds) &&
      bounds.right > bounds.left && bounds.bottom > bounds.top) {
    return bounds;
  }

  return GetVirtualChildBounds();
}

HWND FindWorkerWWindow() {
  const RECT virtualRect = GetVirtualScreenRect();
  const LONG virtualWidth = virtualRect.right - virtualRect.left;
  const LONG virtualHeight = virtualRect.bottom - virtualRect.top;
  const std::int64_t virtualArea =
      virtualWidth > 0 && virtualHeight > 0
          ? static_cast<std::int64_t>(virtualWidth) * static_cast<std::int64_t>(virtualHeight)
          : 0;

  HWND progman = FindWindowW(L"Progman", nullptr);
  if (progman != nullptr) {
    DWORD_PTR result = 0;
    // 向 Progman 发送 0x052C 触发 WorkerW，兼容多版本桌面 shell。
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
  }

  auto areaOf = [](HWND hwnd) -> std::int64_t {
    RECT rc{};
    if (hwnd == nullptr || !GetWindowRect(hwnd, &rc)) {
      return 0;
    }
    const LONG w = rc.right - rc.left;
    const LONG h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return 0;
    }
    return static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h);
  };

  auto isSuitableWorker = [&](HWND hwnd) -> bool {
    if (hwnd == nullptr) {
      return false;
    }
    if (virtualArea <= 0) {
      return true;
    }
    const std::int64_t area = areaOf(hwnd);
    // 至少达到虚拟桌面面积的 50%，避免误选到 136x38 之类的壳窗口。
    return area >= (virtualArea / 2);
  };

  // 优先选择“不承载图标层”的大尺寸 WorkerW。
  HWND bestWorker = nullptr;
  std::int64_t bestArea = 0;
  HWND workerw = nullptr;
  while ((workerw = FindWindowExW(nullptr, workerw, L"WorkerW", nullptr)) != nullptr) {
    const HWND defView = FindWindowExW(workerw, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr) {
      continue;
    }
    if (!isSuitableWorker(workerw)) {
      continue;
    }
    const std::int64_t area = areaOf(workerw);
    if (area > bestArea) {
      bestArea = area;
      bestWorker = workerw;
    }
  }
  if (bestWorker != nullptr) {
    return bestWorker;
  }

  // 兼容路径：找到图标层宿主后取其后继 WorkerW（同样限制为大尺寸窗口）。
  HWND top = nullptr;
  while ((top = FindWindowExW(nullptr, top, nullptr, nullptr)) != nullptr) {
    const HWND defView = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView == nullptr) {
      continue;
    }
    const HWND siblingWorker = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
    if (siblingWorker != nullptr && isSuitableWorker(siblingWorker)) {
      return siblingWorker;
    }
  }
  return nullptr;
}

HWND FindDesktopDefViewWindow() {
  const HWND progman = FindWindowW(L"Progman", nullptr);
  if (progman != nullptr) {
    const HWND defViewOnProgman = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defViewOnProgman != nullptr) {
      return defViewOnProgman;
    }
  }

  HWND top = nullptr;
  while ((top = FindWindowExW(nullptr, top, nullptr, nullptr)) != nullptr) {
    const HWND defView = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr) {
      return defView;
    }
  }
  return nullptr;
}

HWND FindDesktopListViewWindow() {
  const HWND defView = FindDesktopDefViewWindow();
  if (defView == nullptr) {
    return nullptr;
  }
  return FindWindowExW(defView, nullptr, L"SysListView32", nullptr);
}

void EnsureDesktopIconLayerVisible() {
  const HWND defView = FindDesktopDefViewWindow();
  if (defView != nullptr) {
    SetWindowPos(defView, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    ShowWindow(defView, SW_SHOWNOACTIVATE);
  }

  const HWND listView = FindDesktopListViewWindow();
  if (listView == nullptr) {
    return;
  }
  ShowWindow(listView, SW_SHOWNOACTIVATE);
  SetWindowPos(listView, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
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

    desktopParent_ = nullptr;
    RECT bounds{};

    const auto tryCreateForParent = [&](HWND parent) -> HWND {
      if (parent == nullptr) {
        return nullptr;
      }
      return CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, kWallpaperWindowClassName, L"",
                             WS_CHILD,
                             0, 0, bounds.right - bounds.left,
                             bounds.bottom - bounds.top, parent, nullptr, instance_, nullptr);
    };

    const HWND preferredWorker = FindWorkerWWindow();
    const HWND defView = FindDesktopDefViewWindow();
    const auto targets = BuildDesktopAttachOrder(preferredWorker != nullptr, defView != nullptr);
    for (const DesktopAttachTarget target : targets) {
      HWND candidate = nullptr;
      if (target == DesktopAttachTarget::kWorkerW) {
        candidate = preferredWorker;
      } else if (target == DesktopAttachTarget::kDefView) {
        candidate = defView;
      }
      if (candidate == nullptr) {
        continue;
      }
      bounds = GetParentBoundsOrVirtual(candidate);
      renderWindow_ = tryCreateForParent(candidate);
      if (renderWindow_ != nullptr) {
        desktopParent_ = candidate;
        break;
      }
    }

    if (renderWindow_ == nullptr) {
      return false;
    }

    if (!InitializeD3D()) {
      DestroyWindow(renderWindow_);
      renderWindow_ = nullptr;
      return false;
    }

    SetWindowPos(renderWindow_, HWND_BOTTOM, 0, 0, bounds.right - bounds.left,
                 bounds.bottom - bounds.top, SWP_NOACTIVATE);
    EnsureDesktopIconLayerVisible();
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
    renderWindowVisible_ = false;
  }

  void ResizeForDisplays() override {
    if (renderWindow_ == nullptr) {
      return;
    }

    const RECT bounds = GetParentBoundsOrVirtual(desktopParent_);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
      return;
    }

    SetWindowPos(renderWindow_, HWND_BOTTOM, 0, 0, width, height, SWP_NOACTIVATE);
    EnsureDesktopIconLayerVisible();
    ResizeSwapChain(static_cast<UINT>(width), static_cast<UINT>(height));
  }

  void Present(const FrameToken& frame) override {
    if (context_ == nullptr || renderTargetView_ == nullptr || swapChain_ == nullptr) {
      return;
    }
    if (frameLatencyWaitableEnabled_ && frameLatencyWaitableObject_ != nullptr) {
      const DWORD waitResult = WaitForSingleObjectEx(frameLatencyWaitableObject_, 0, FALSE);
      if (waitResult == WAIT_TIMEOUT) {
        // DXGI 队列仍在消费上一帧时跳过本帧，减少无效 CPU/GPU 提交。
        return;
      }
    }

    if (frame.decodeMode != DecodeMode::kMediaFoundation && videoTexture_ != nullptr) {
      // 非视频解码模式下清理旧视频纹理，避免切换源后显示冻结旧帧。
      ReleaseVideoTexture();
      lastVideoSequence_ = 0;
    }

    bool drewVideo = false;
    bool hasVideoTexture = videoSrv_ != nullptr && videoTexture_ != nullptr;
    frame_bridge::LatestFrame latestFrame;
    if (frame_bridge::TryGetLatestFrame(&latestFrame) &&
        latestFrame.rgbaPixels != nullptr && latestFrame.sequence != lastVideoSequence_) {
      if (EnsureVideoTexture(static_cast<UINT>(latestFrame.width),
                             static_cast<UINT>(latestFrame.height)) &&
          UploadVideoFrame(latestFrame) && DrawVideoTexture()) {
        lastVideoSequence_ = latestFrame.sequence;
        hasVideoTexture = true;
        drewVideo = true;
      }
    }

    if (!drewVideo && hasVideoTexture && DrawVideoTexture()) {
      drewVideo = true;
    }

    if (!drewVideo) {
      return;
    }

    if (!renderWindowVisible_) {
      ShowWindow(renderWindow_, SW_SHOWNOACTIVATE);
      renderWindowVisible_ = true;
      EnsureDesktopIconLayerVisible();
    }

    swapChain_->Present(1, 0);
  }

 private:
  bool InitializeD3D() {
    // 单线程渲染路径启用 single-threaded 设备，减少线程安全开销。
    // 约束：本项目所有 D3D 设备/上下文调用均在主线程执行。
    UINT createFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    const UINT levelCount = static_cast<UINT>(sizeof(levels) / sizeof(levels[0]));

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                                   levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
    if (FAILED(hr)) {
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, levels,
                             levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
      if (FAILED(hr)) {
        return false;
      }
    }

    IDXGIDevice1* dxgiDevice1 = nullptr;
    if (SUCCEEDED(device_->QueryInterface(__uuidof(IDXGIDevice1),
                                          reinterpret_cast<void**>(&dxgiDevice1))) &&
        dxgiDevice1 != nullptr) {
      // 将 DXGI 预渲染队列限制为 1，降低排队帧导致的额外内存与延迟开销。
      dxgiDevice1->SetMaximumFrameLatency(1);
      SafeRelease(&dxgiDevice1);
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
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    hr = factory->CreateSwapChainForHwnd(device_, renderWindow_, &desc, nullptr, nullptr, &swapChain_);
    if (FAILED(hr)) {
      // 兼容回退：部分旧驱动可能不支持 waitable-object 标志。
      desc.Flags = 0;
      hr = factory->CreateSwapChainForHwnd(device_, renderWindow_, &desc, nullptr, nullptr,
                                           &swapChain_);
    }
    if (FAILED(hr)) {
      // 兼容回退：部分旧驱动可能不支持 FLIP_DISCARD。
      desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      hr = factory->CreateSwapChainForHwnd(device_, renderWindow_, &desc, nullptr, nullptr,
                                           &swapChain_);
    }
    SafeRelease(&factory);
    if (FAILED(hr)) {
      return false;
    }

    frameLatencyWaitableEnabled_ = false;
    frameLatencyWaitableObject_ = nullptr;
    SafeRelease(&swapChain2_);
    if (SUCCEEDED(swapChain_->QueryInterface(__uuidof(IDXGISwapChain2),
                                             reinterpret_cast<void**>(&swapChain2_))) &&
        swapChain2_ != nullptr) {
      if (SUCCEEDED(swapChain2_->SetMaximumFrameLatency(1))) {
        frameLatencyWaitableObject_ = swapChain2_->GetFrameLatencyWaitableObject();
        frameLatencyWaitableEnabled_ = frameLatencyWaitableObject_ != nullptr;
      }
    }

    if (!CreateRenderTargetView()) {
      return false;
    }

    // 视频渲染管线创建失败时仍保留色场路径，避免影响基本可用性。
    CreateVideoPipeline();
    return true;
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
    return SUCCEEDED(hr);
  }

  bool CreateVideoPipeline() {
    if (device_ == nullptr) {
      return false;
    }

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    if (!CompileShader(kVideoVsSource, "main", "vs_4_0", &vsBlob) ||
        !CompileShader(kVideoPsSource, "main", "ps_4_0", &psBlob)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      return false;
    }

    HRESULT hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                             nullptr, &videoVertexShader_);
    if (FAILED(hr)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      return false;
    }

    hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                    &videoPixelShader_);
    if (FAILED(hr)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      return false;
    }

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = device_->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)),
                                    vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                    &videoInputLayout_);
    SafeRelease(&vsBlob);
    SafeRelease(&psBlob);
    if (FAILED(hr)) {
      return false;
    }

    const VideoVertex vertices[] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},
        {-1.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, -1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 0.0f},
    };

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(vertices));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;

    hr = device_->CreateBuffer(&vbDesc, &vbData, &videoVertexBuffer_);
    if (FAILED(hr)) {
      return false;
    }

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&samplerDesc, &videoSampler_);
    if (FAILED(hr)) {
      return false;
    }

    videoPipelineReady_ = true;
    return true;
  }

  void ReleaseVideoPipeline() {
    videoPipelineReady_ = false;
    SafeRelease(&videoSampler_);
    SafeRelease(&videoVertexBuffer_);
    SafeRelease(&videoInputLayout_);
    SafeRelease(&videoPixelShader_);
    SafeRelease(&videoVertexShader_);
  }

  void ReleaseVideoTexture() {
    SafeRelease(&videoSrv_);
    SafeRelease(&videoTexture_);
    videoTexWidth_ = 0;
    videoTexHeight_ = 0;
  }

  bool EnsureVideoTexture(const UINT width, const UINT height) {
    if (!videoPipelineReady_ || device_ == nullptr || width == 0 || height == 0) {
      return false;
    }

    if (videoTexture_ != nullptr && videoSrv_ != nullptr &&
        videoTexWidth_ == width && videoTexHeight_ == height) {
      return true;
    }

    ReleaseVideoTexture();

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device_->CreateTexture2D(&texDesc, nullptr, &videoTexture_);
    if (FAILED(hr)) {
      return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(videoTexture_, &srvDesc, &videoSrv_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    videoTexWidth_ = width;
    videoTexHeight_ = height;
    return true;
  }

  bool UploadVideoFrame(const frame_bridge::LatestFrame& frame) {
    if (context_ == nullptr || videoTexture_ == nullptr || frame.rgbaPixels == nullptr) {
      return false;
    }

    if (frame.width <= 0 || frame.height <= 0 || frame.strideBytes <= 0) {
      return false;
    }

    const auto& pixels = *frame.rgbaPixels;
    const std::size_t minBytes = static_cast<std::size_t>(frame.strideBytes) *
                                 static_cast<std::size_t>(frame.height);
    if (pixels.size() < minBytes) {
      return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(videoTexture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) ||
        mapped.pData == nullptr || mapped.RowPitch == 0) {
      return false;
    }

    const auto* src = pixels.data();
    auto* dst = static_cast<std::uint8_t*>(mapped.pData);
    const UINT srcRowPitch = static_cast<UINT>(frame.strideBytes);
    const UINT copyBytes = srcRowPitch < mapped.RowPitch ? srcRowPitch : mapped.RowPitch;
    for (int row = 0; row < frame.height; ++row) {
      std::memcpy(dst + static_cast<std::size_t>(row) * mapped.RowPitch,
                  src + static_cast<std::size_t>(row) * srcRowPitch, copyBytes);
    }
    context_->Unmap(videoTexture_, 0);
    return true;
  }

  bool DrawVideoTexture() {
    if (!videoPipelineReady_ || context_ == nullptr || renderTargetView_ == nullptr ||
        videoSrv_ == nullptr || videoSampler_ == nullptr || videoInputLayout_ == nullptr ||
        videoVertexBuffer_ == nullptr || videoVertexShader_ == nullptr ||
        videoPixelShader_ == nullptr) {
      return false;
    }

    RECT rect{};
    GetClientRect(renderWindow_, &rect);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(rect.right - rect.left);
    viewport.Height = static_cast<float>(rect.bottom - rect.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);

    context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);

    const UINT stride = sizeof(VideoVertex);
    const UINT offset = 0;
    context_->IASetInputLayout(videoInputLayout_);
    context_->IASetVertexBuffers(0, 1, &videoVertexBuffer_, &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(videoVertexShader_, nullptr, 0);
    context_->PSSetShader(videoPixelShader_, nullptr, 0);
    context_->PSSetSamplers(0, 1, &videoSampler_);
    context_->PSSetShaderResources(0, 1, &videoSrv_);
    context_->Draw(4, 0);

    ID3D11ShaderResourceView* nullSrv = nullptr;
    context_->PSSetShaderResources(0, 1, &nullSrv);
    return true;
  }

  void DrawFallback(const FrameToken& frame) {
    const float phase = static_cast<float>(frame.sequence % 6000ULL) * 0.0020f;
    // 使用低频正弦变化产生平滑动态色彩，确保即使无视频输入也有可见动态效果。
    const float red = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase));
    const float green = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase + 2.10f));
    const float blue = 0.20f + 0.30f * (0.5f + 0.5f * std::sin(phase + 4.20f));
    const float color[4] = {red, green, blue, 1.0f};

    context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->ClearRenderTargetView(renderTargetView_, color);
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
    frameLatencyWaitableEnabled_ = false;
    if (frameLatencyWaitableObject_ != nullptr) {
      CloseHandle(frameLatencyWaitableObject_);
      frameLatencyWaitableObject_ = nullptr;
    }
    SafeRelease(&swapChain2_);
    ReleaseVideoTexture();
    ReleaseVideoPipeline();
    SafeRelease(&renderTargetView_);
    SafeRelease(&swapChain_);
    SafeRelease(&context_);
    SafeRelease(&device_);
  }

  HINSTANCE instance_ = nullptr;
  HWND desktopParent_ = nullptr;
  HWND renderWindow_ = nullptr;
  bool attached_ = false;
  bool renderWindowVisible_ = false;

  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  IDXGISwapChain1* swapChain_ = nullptr;
  IDXGISwapChain2* swapChain2_ = nullptr;
  HANDLE frameLatencyWaitableObject_ = nullptr;
  bool frameLatencyWaitableEnabled_ = false;
  ID3D11RenderTargetView* renderTargetView_ = nullptr;

  bool videoPipelineReady_ = false;
  ID3D11VertexShader* videoVertexShader_ = nullptr;
  ID3D11PixelShader* videoPixelShader_ = nullptr;
  ID3D11InputLayout* videoInputLayout_ = nullptr;
  ID3D11Buffer* videoVertexBuffer_ = nullptr;
  ID3D11SamplerState* videoSampler_ = nullptr;
  ID3D11Texture2D* videoTexture_ = nullptr;
  ID3D11ShaderResourceView* videoSrv_ = nullptr;
  UINT videoTexWidth_ = 0;
  UINT videoTexHeight_ = 0;
  std::uint64_t lastVideoSequence_ = 0;
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
