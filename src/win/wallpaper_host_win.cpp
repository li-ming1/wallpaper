#include "wallpaper/interfaces.h"
#include "wallpaper/compiler_assume.h"
#include "wallpaper/d3d11_interop_device.h"
#include "wallpaper/frame_bridge.h"
#include "wallpaper/desktop_attach_policy.h"
#include "wallpaper/monitor_layout_policy.h"
#include "wallpaper/runtime_trim_policy.h"
#include "wallpaper/swap_chain_policy.h"
#include "wallpaper/upload_copy_policy.h"
#include "wallpaper/upload_scale_policy.h"
#include "wallpaper/upload_texture_policy.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <psapi.h>

#include <cstdint>
#include <array>
#include <cstring>
#include <iterator>
#include <span>
#include <vector>

namespace wallpaper {

namespace {

template <typename T>
void SafeRelease(T** ptr) {
  if (ptr != nullptr && *ptr != nullptr) {
    (*ptr)->Release();
    *ptr = nullptr;
  }
}

constexpr wchar_t kWallpaperWindowClassName[] = L"WallpaperRenderHostWindow";
constexpr UINT_PTR kAppIconResourceId = 1;

HICON LoadAppIcon(HINSTANCE instance, const int width, const int height) {
  if (instance == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kAppIconResourceId),
                                            IMAGE_ICON, width, height,
                                            LR_DEFAULTCOLOR | LR_SHARED));
}

struct VideoVertex final {
  float x;
  float y;
  float u;
  float v;
};

consteval std::array<VideoVertex, 4> BuildFullscreenQuadVertices() {
  return {{{-1.0f, -1.0f, 0.0f, 1.0f},
           {-1.0f, 1.0f, 0.0f, 0.0f},
           {1.0f, -1.0f, 1.0f, 1.0f},
           {1.0f, 1.0f, 1.0f, 0.0f}}};
}

constexpr std::array<VideoVertex, 4> kFullscreenQuadVertices = BuildFullscreenQuadVertices();

void CopyLinearRows(const std::span<const std::uint8_t> srcBytes, const UINT srcRowPitch,
                    std::span<std::uint8_t> dstBytes, const UINT dstRowPitch, const int rowCount,
                    const UINT rowCopyBytes) {
  if (rowCount <= 0 || srcRowPitch == 0 || dstRowPitch == 0 || rowCopyBytes == 0) {
    return;
  }
  for (int row = 0; row < rowCount; ++row) {
    const std::size_t srcOffset = static_cast<std::size_t>(row) * srcRowPitch;
    const std::size_t dstOffset = static_cast<std::size_t>(row) * dstRowPitch;
    const auto srcRow = srcBytes.subspan(srcOffset, rowCopyBytes);
    auto dstRow = dstBytes.subspan(dstOffset, rowCopyBytes);
    std::memcpy(dstRow.data(), srcRow.data(), rowCopyBytes);
  }
}

struct ResolvedUploadScalePlan final {
  UINT targetWidth = 0;
  UINT targetHeight = 0;
  bool scaled = false;
};

[[nodiscard]] ResolvedUploadScalePlan ResolveUploadScalePlan(
    const FrameToken& frame, const frame_bridge::LatestFrame& latestFrame) noexcept {
  if (latestFrame.width <= 0 || latestFrame.height <= 0) {
    return {};
  }

  ResolvedUploadScalePlan plan;
  plan.targetWidth = static_cast<UINT>(latestFrame.width);
  plan.targetHeight = static_cast<UINT>(latestFrame.height);
  if (!IsCpuFallbackDecodePath(frame.decodePath)) {
    return plan;
  }

  const UploadScalePlan selected =
      SelectUploadScalePlanForCpuUpload(latestFrame.width, latestFrame.height);
  if (selected.targetWidth <= 0 || selected.targetHeight <= 0) {
    return plan;
  }

  plan.targetWidth = static_cast<UINT>(selected.targetWidth);
  plan.targetHeight = static_cast<UINT>(selected.targetHeight);
  plan.scaled = plan.targetWidth != static_cast<UINT>(latestFrame.width) ||
                plan.targetHeight != static_cast<UINT>(latestFrame.height);
  return plan;
}

void DownscaleRgbaNearest(const std::uint8_t* const srcBase, const UINT srcRowPitch,
                          const int srcWidth, const int srcHeight, std::uint8_t* const dstBase,
                          const UINT dstRowPitch, const UINT targetWidth,
                          const UINT targetHeight) {
  if (srcBase == nullptr || dstBase == nullptr || srcRowPitch == 0 || dstRowPitch == 0 ||
      srcWidth <= 0 || srcHeight <= 0 || targetWidth == 0 || targetHeight == 0) {
    return;
  }

  // CPU 回退上传缩放使用最近邻采样：实现简单、可预测，优先守住上传带宽与驻留预算。
  for (UINT y = 0; y < targetHeight; ++y) {
    const int srcY = static_cast<int>((static_cast<std::uint64_t>(y) * srcHeight) / targetHeight);
    const auto* srcRow = srcBase + static_cast<std::size_t>(srcY) * srcRowPitch;
    auto* dstRow = dstBase + static_cast<std::size_t>(y) * dstRowPitch;
    for (UINT x = 0; x < targetWidth; ++x) {
      const int srcX =
          static_cast<int>((static_cast<std::uint64_t>(x) * srcWidth) / targetWidth);
      const auto* srcPixel = srcRow + static_cast<std::size_t>(srcX) * 4U;
      auto* dstPixel = dstRow + static_cast<std::size_t>(x) * 4U;
      std::memcpy(dstPixel, srcPixel, 4U);
    }
  }
}

void DownscalePlaneNearest(const std::uint8_t* const srcBase, const UINT srcRowPitch,
                           const int srcWidth, const int srcHeight, std::uint8_t* const dstBase,
                           const UINT dstRowPitch, const UINT targetWidth,
                           const UINT targetHeight) {
  if (srcBase == nullptr || dstBase == nullptr || srcRowPitch == 0 || dstRowPitch == 0 ||
      srcWidth <= 0 || srcHeight <= 0 || targetWidth == 0 || targetHeight == 0) {
    return;
  }

  for (UINT y = 0; y < targetHeight; ++y) {
    const int srcY = static_cast<int>((static_cast<std::uint64_t>(y) * srcHeight) / targetHeight);
    const auto* srcRow = srcBase + static_cast<std::size_t>(srcY) * srcRowPitch;
    auto* dstRow = dstBase + static_cast<std::size_t>(y) * dstRowPitch;
    for (UINT x = 0; x < targetWidth; ++x) {
      const int srcX =
          static_cast<int>((static_cast<std::uint64_t>(x) * srcWidth) / targetWidth);
      dstRow[x] = srcRow[srcX];
    }
  }
}

void DownscaleInterleavedUvNearest(const std::uint8_t* const srcBase, const UINT srcRowPitch,
                                   const int srcWidthSamples, const int srcHeightSamples,
                                   std::uint8_t* const dstBase, const UINT dstRowPitch,
                                   const UINT targetWidthSamples,
                                   const UINT targetHeightSamples) {
  if (srcBase == nullptr || dstBase == nullptr || srcRowPitch == 0 || dstRowPitch == 0 ||
      srcWidthSamples <= 0 || srcHeightSamples <= 0 || targetWidthSamples == 0 ||
      targetHeightSamples == 0) {
    return;
  }

  for (UINT y = 0; y < targetHeightSamples; ++y) {
    const int srcY =
        static_cast<int>((static_cast<std::uint64_t>(y) * srcHeightSamples) / targetHeightSamples);
    const auto* srcRow = srcBase + static_cast<std::size_t>(srcY) * srcRowPitch;
    auto* dstRow = dstBase + static_cast<std::size_t>(y) * dstRowPitch;
    for (UINT x = 0; x < targetWidthSamples; ++x) {
      const int srcX = static_cast<int>(
          (static_cast<std::uint64_t>(x) * srcWidthSamples) / targetWidthSamples);
      const auto* srcPixel = srcRow + static_cast<std::size_t>(srcX) * 2U;
      auto* dstPixel = dstRow + static_cast<std::size_t>(x) * 2U;
      dstPixel[0] = srcPixel[0];
      dstPixel[1] = srcPixel[1];
    }
  }
}

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

constexpr char kVideoNv12PsSource[] = R"(
Texture2D lumaTex : register(t0);
Texture2D chromaTex : register(t1);
SamplerState videoSamp : register(s0);

struct PSIn {
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET {
  const float ySample = lumaTex.Sample(videoSamp, input.uv).r;
  const float2 uvSample = chromaTex.Sample(videoSamp, input.uv).rg;

  const float y = max(0.0, 1.16438356 * (ySample - 0.06274510));
  const float u = uvSample.x - 0.5;
  const float v = uvSample.y - 0.5;

  float3 rgb;
  rgb.r = saturate(y + 1.79274107 * v);
  rgb.g = saturate(y - 0.21324861 * u - 0.53290933 * v);
  rgb.b = saturate(y + 2.11240179 * u);
  return float4(rgb, 1.0);
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
  const int largeW = GetSystemMetrics(SM_CXICON);
  const int largeH = GetSystemMetrics(SM_CYICON);
  const int smallW = GetSystemMetrics(SM_CXSMICON);
  const int smallH = GetSystemMetrics(SM_CYSMICON);
  wc.hIcon = LoadAppIcon(instance, largeW, largeH);
  if (wc.hIcon == nullptr) {
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  }
  wc.hIconSm = LoadAppIcon(instance, smallW, smallH);
  if (wc.hIconSm == nullptr) {
    wc.hIconSm = wc.hIcon;
  }
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

DisplayRect ToDisplayRect(const RECT rect) {
  return DisplayRect{rect.left, rect.top, rect.right, rect.bottom};
}

RenderViewport BuildFullscreenViewport(const UINT width, const UINT height) {
  return RenderViewport{
      0,
      0,
      static_cast<int>(width),
      static_cast<int>(height),
  };
}

DXGI_SWAP_EFFECT ToDxgiSwapEffect(const SwapChainEffect effect) {
  switch (effect) {
    case SwapChainEffect::kDiscard:
      return DXGI_SWAP_EFFECT_DISCARD;
    case SwapChainEffect::kSequential:
      return DXGI_SWAP_EFFECT_SEQUENTIAL;
    case SwapChainEffect::kFlipSequential:
      return DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    case SwapChainEffect::kFlipDiscard:
    default:
      return DXGI_SWAP_EFFECT_FLIP_DISCARD;
  }
}

bool TrimCurrentProcessWorkingSetNow() {
  return EmptyWorkingSet(GetCurrentProcess()) != FALSE;
}

std::size_t QueryCurrentProcessWorkingSetBytes() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                            counters.cb)) {
    return 0;
  }
  return static_cast<std::size_t>(counters.WorkingSetSize);
}

BOOL CALLBACK CollectMonitorRect(HMONITOR, HDC, LPRECT monitorRect, LPARAM userData) {
  if (monitorRect == nullptr || userData == 0) {
    return TRUE;
  }
  auto* monitors = reinterpret_cast<std::vector<DisplayRect>*>(userData);
  const DisplayRect rect = ToDisplayRect(*monitorRect);
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return TRUE;
  }
  monitors->push_back(rect);
  return TRUE;
}

std::vector<DisplayRect> EnumerateMonitorRects() {
  std::vector<DisplayRect> monitors;
  EnumDisplayMonitors(nullptr, nullptr, CollectMonitorRect,
                      reinterpret_cast<LPARAM>(&monitors));
  return monitors;
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
      ReleaseD3D();
      DestroyWindow(renderWindow_);
      renderWindow_ = nullptr;
      return false;
    }

    SetWindowPos(renderWindow_, HWND_BOTTOM, 0, 0, bounds.right - bounds.left,
                 bounds.bottom - bounds.top, SWP_NOACTIVATE);
    UpdateRenderViewportCache(static_cast<UINT>(bounds.right - bounds.left),
                              static_cast<UINT>(bounds.bottom - bounds.top));
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
    renderViewportWidth_ = 0;
    renderViewportHeight_ = 0;
    monitorViewports_.clear();
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
    UpdateRenderViewportCache(static_cast<UINT>(width), static_cast<UINT>(height));
    EnsureDesktopIconLayerVisible();
    ResizeSwapChain(static_cast<UINT>(width), static_cast<UINT>(height));
  }

  void Present(const FrameToken& frame) override {
    if (context_ == nullptr || renderTargetView_ == nullptr || swapChain_ == nullptr) {
      return;
    }

    const ULONGLONG nowTick = GetTickCount64();
    if (swapChainOccluded_) {
      if (nowTick < nextOcclusionProbeAtTick_) {
        return;
      }
      const HRESULT probeHr = swapChain_->Present(0, DXGI_PRESENT_TEST);
      if (probeHr == DXGI_STATUS_OCCLUDED) {
        nextOcclusionProbeAtTick_ = nowTick + kOcclusionProbeIntervalMs;
        return;
      }
      swapChainOccluded_ = false;
      nextOcclusionProbeAtTick_ = 0;
    }

    if (frame.decodeMode != DecodeMode::kMediaFoundation && HasAnyVideoTexture()) {
      // 非视频解码模式下清理旧视频纹理，避免切换源后显示冻结旧帧。
      ReleaseVideoTexture();
      lastVideoSequence_ = 0;
    }

    bool drewVideo = false;
    bool hasVideoTexture = HasAnyVideoTexture();
    frame_bridge::LatestFrame latestFrame;
    if (frame_bridge::TryGetLatestFrameIfNewer(lastVideoSequence_, &latestFrame)) {
      const ResolvedUploadScalePlan uploadScalePlan = ResolveUploadScalePlan(frame, latestFrame);
      const std::size_t uploadPixels =
          static_cast<std::size_t>(uploadScalePlan.targetWidth) *
          static_cast<std::size_t>(uploadScalePlan.targetHeight);
      const bool useDefaultTextureUpload = ShouldUseDefaultTextureUpload(
          frame.decodePath, uploadPixels, uploadScalePlan.scaled);
      if (latestFrame.gpuBacked &&
          latestFrame.pixelFormat == frame_bridge::PixelFormat::kNv12 &&
          latestFrame.gpuTexture != nullptr && latestFrame.gpuAuxTexture != nullptr &&
          EnsureVideoTextureForExternalNv12(latestFrame) && DrawVideoNv12Texture()) {
        lastVideoSequence_ = latestFrame.sequence;
        frame_bridge::ReleaseLatestFrameIfSequenceConsumed(lastVideoSequence_);
        hasVideoTexture = true;
        drewVideo = true;
      } else if (latestFrame.gpuBacked && latestFrame.gpuTexture != nullptr &&
                 EnsureVideoTextureForGpu(static_cast<UINT>(latestFrame.width),
                                          static_cast<UINT>(latestFrame.height),
                                          static_cast<DXGI_FORMAT>(latestFrame.dxgiFormat)) &&
          CopyGpuVideoFrame(latestFrame) && DrawVideoTexture()) {
        lastVideoSequence_ = latestFrame.sequence;
        frame_bridge::ReleaseLatestFrameIfSequenceConsumed(lastVideoSequence_);
        hasVideoTexture = true;
        drewVideo = true;
      } else if (latestFrame.pixelFormat == frame_bridge::PixelFormat::kNv12 &&
                 latestFrame.yPlaneData != nullptr && latestFrame.uvPlaneData != nullptr &&
                 EnsureVideoTextureForNv12(uploadScalePlan.targetWidth,
                                           uploadScalePlan.targetHeight,
                                           !useDefaultTextureUpload) &&
                 UploadVideoFrameNv12(latestFrame, uploadScalePlan) && DrawVideoNv12Texture()) {
        lastVideoSequence_ = latestFrame.sequence;
        frame_bridge::ReleaseLatestFrameIfSequenceConsumed(lastVideoSequence_);
        hasVideoTexture = true;
        drewVideo = true;
      } else if (latestFrame.rgbaData != nullptr &&
                 EnsureVideoTextureForCpu(uploadScalePlan.targetWidth,
                                          uploadScalePlan.targetHeight,
                                          !useDefaultTextureUpload) &&
                 UploadVideoFrame(latestFrame, uploadScalePlan) && DrawVideoTexture()) {
        lastVideoSequence_ = latestFrame.sequence;
        frame_bridge::ReleaseLatestFrameIfSequenceConsumed(lastVideoSequence_);
        hasVideoTexture = true;
        drewVideo = true;
      }
    }

    if (!drewVideo && hasVideoTexture && DrawCachedVideoTexture()) {
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

    const HRESULT presentHr = swapChain_->Present(1, 0);
    if (presentHr == DXGI_STATUS_OCCLUDED) {
      swapChainOccluded_ = true;
      nextOcclusionProbeAtTick_ = nowTick + kOcclusionProbeIntervalMs;
      return;
    }
    if (SUCCEEDED(presentHr)) {
      swapChainOccluded_ = false;
      nextOcclusionProbeAtTick_ = 0;
      const std::size_t decodeOutputPixels =
          frame.width > 0 && frame.height > 0
              ? static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height)
              : 0U;
      const auto postPresentTrimInterval = SelectPostPresentWorkingSetTrimInterval(
          true, frame.decodePath, decodeOutputPixels);
      const std::size_t postPresentTrimThresholdBytes =
          SelectPostPresentWorkingSetTrimThresholdBytes(true, frame.decodePath,
                                                        decodeOutputPixels);
      const bool postPresentTrimDue =
          postPresentTrimInterval.count() > 0 &&
          (lastPostPresentTrimAtTick_ == 0 ||
           nowTick - lastPostPresentTrimAtTick_ >=
               static_cast<ULONGLONG>(postPresentTrimInterval.count()));
      if (postPresentTrimDue) {
        if (postPresentTrimThresholdBytes != 0) {
          const std::size_t workingSetBytes = QueryCurrentProcessWorkingSetBytes();
          if (workingSetBytes < postPresentTrimThresholdBytes) {
            return;
          }
        }
        // 紧凑 CPU fallback 的峰值主要来自活跃渲染触达页；present 成功后立即回收，
        // 但必须限制频率，避免 working-set 回收本身反向把 CPU 顶上去。
        TrimCurrentProcessWorkingSetNow();
        lastPostPresentTrimAtTick_ = nowTick;
      }
    }
  }

  [[nodiscard]] bool IsOccluded() const override { return swapChainOccluded_; }

 private:
  void UpdateRenderViewportCache(const UINT width, const UINT height) {
    if (width == 0 || height == 0) {
      return;
    }
    renderViewportWidth_ = width;
    renderViewportHeight_ = height;
    RebuildMonitorViewports();
  }

  void RebuildMonitorViewports() {
    monitorViewports_.clear();
    if (renderViewportWidth_ == 0 || renderViewportHeight_ == 0) {
      return;
    }

    const RECT virtualRect = GetVirtualScreenRect();
    const std::vector<DisplayRect> monitors = EnumerateMonitorRects();
    monitorViewports_ = BuildScaledRenderMonitorViewports(
        ToDisplayRect(virtualRect), monitors, static_cast<int>(renderViewportWidth_),
        static_cast<int>(renderViewportHeight_));
    if (monitorViewports_.empty()) {
      // 兜底回退到单视口，确保显示链路可用。
      monitorViewports_.push_back(
          BuildFullscreenViewport(renderViewportWidth_, renderViewportHeight_));
    }
  }

  bool RefreshRenderViewportCacheFromWindow() {
    if (renderWindow_ == nullptr) {
      return false;
    }
    RECT rect{};
    if (!GetClientRect(renderWindow_, &rect)) {
      return false;
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
      return false;
    }
    UpdateRenderViewportCache(static_cast<UINT>(width), static_cast<UINT>(height));
    return true;
  }

  bool InitializeD3D() {
    // 设备同时用于渲染与解码互操作，需保持线程安全能力。
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    const UINT levelCount = static_cast<UINT>(sizeof(levels) / sizeof(levels[0]));

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                                   levelCount, D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
    if (FAILED(hr)) {
      const UINT warpFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, warpFlags, levels, levelCount,
                             D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
      if (FAILED(hr)) {
        return false;
      }
    }

    ID3D10Multithread* multithread = nullptr;
    if (SUCCEEDED(device_->QueryInterface(__uuidof(ID3D10Multithread),
                                          reinterpret_cast<void**>(&multithread))) &&
        multithread != nullptr) {
      multithread->SetMultithreadProtected(TRUE);
      SafeRelease(&multithread);
    }

    // 解码与渲染共享同一 D3D11 设备，供 MF SourceReader 走 D3D 互操作零拷贝路径。
    d3d11_interop::SetSharedDevice(device_);

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

    if (!RefreshRenderViewportCacheFromWindow()) {
      const RECT fallbackBounds = GetVirtualChildBounds();
      const int fallbackWidth = fallbackBounds.right - fallbackBounds.left;
      const int fallbackHeight = fallbackBounds.bottom - fallbackBounds.top;
      if (fallbackWidth > 0 && fallbackHeight > 0) {
        UpdateRenderViewportCache(static_cast<UINT>(fallbackWidth),
                                  static_cast<UINT>(fallbackHeight));
      }
    }
    const UINT width = renderViewportWidth_ > 0 ? renderViewportWidth_ : 1280U;
    const UINT height = renderViewportHeight_ > 0 ? renderViewportHeight_ : 720U;

    hr = E_FAIL;
    for (const SwapChainPlan& plan : BuildSwapChainPlans()) {
      DXGI_SWAP_CHAIN_DESC1 desc{};
      desc.Width = width;
      desc.Height = height;
      desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      desc.SampleDesc.Count = 1;
      desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      desc.BufferCount = plan.bufferCount;
      desc.SwapEffect = ToDxgiSwapEffect(plan.effect);
      desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
      desc.Scaling = DXGI_SCALING_STRETCH;
      desc.Flags = 0U;

      // 优先尝试单缓冲 blt-model，目标是降低常驻页；失败后再回退到现有 flip 链路。
      hr = factory->CreateSwapChainForHwnd(device_, renderWindow_, &desc, nullptr, nullptr,
                                           &swapChain_);
      if (SUCCEEDED(hr) && swapChain_ != nullptr) {
        break;
      }
    }
    SafeRelease(&factory);
    if (FAILED(hr)) {
      return false;
    }

    swapChainOccluded_ = false;
    nextOcclusionProbeAtTick_ = 0;
    SafeRelease(&swapChain2_);
    if (SUCCEEDED(swapChain_->QueryInterface(__uuidof(IDXGISwapChain2),
                                             reinterpret_cast<void**>(&swapChain2_))) &&
        swapChain2_ != nullptr) {
      swapChain2_->SetMaximumFrameLatency(1);
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
    ID3DBlob* nv12PsBlob = nullptr;
    if (!CompileShader(kVideoVsSource, "main", "vs_4_0", &vsBlob) ||
        !CompileShader(kVideoPsSource, "main", "ps_4_0", &psBlob) ||
        !CompileShader(kVideoNv12PsSource, "main", "ps_4_0", &nv12PsBlob)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      SafeRelease(&nv12PsBlob);
      return false;
    }

    HRESULT hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                             nullptr, &videoVertexShader_);
    if (FAILED(hr)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      SafeRelease(&nv12PsBlob);
      return false;
    }

    hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                    &videoPixelShader_);
    if (FAILED(hr)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      SafeRelease(&nv12PsBlob);
      return false;
    }

    hr = device_->CreatePixelShader(nv12PsBlob->GetBufferPointer(), nv12PsBlob->GetBufferSize(),
                                    nullptr, &videoNv12PixelShader_);
    if (FAILED(hr)) {
      SafeRelease(&vsBlob);
      SafeRelease(&psBlob);
      SafeRelease(&nv12PsBlob);
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
    SafeRelease(&nv12PsBlob);
    if (FAILED(hr)) {
      return false;
    }

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(kFullscreenQuadVertices));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = kFullscreenQuadVertices.data();

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
    SafeRelease(&videoNv12PixelShader_);
    SafeRelease(&videoPixelShader_);
    SafeRelease(&videoVertexShader_);
  }

  [[nodiscard]] bool HasRgbaVideoTexture() const {
    return videoTexture_ != nullptr && videoSrv_ != nullptr;
  }

  [[nodiscard]] bool HasNv12VideoTexture() const {
    return videoNv12YTexture_ != nullptr && videoNv12UvTexture_ != nullptr &&
           videoNv12YSrv_ != nullptr && videoNv12UvSrv_ != nullptr;
  }

  [[nodiscard]] bool HasAnyVideoTexture() const {
    return HasRgbaVideoTexture() || HasNv12VideoTexture();
  }

  void ReleaseVideoTexture() {
    SafeRelease(&videoSrv_);
    SafeRelease(&videoTexture_);
    videoTexWidth_ = 0;
    videoTexHeight_ = 0;
    videoTexFormat_ = DXGI_FORMAT_UNKNOWN;
    videoTextureCpuWritable_ = false;
    SafeRelease(&videoNv12YSrv_);
    SafeRelease(&videoNv12UvSrv_);
    SafeRelease(&videoNv12YTexture_);
    SafeRelease(&videoNv12UvTexture_);
    videoNv12Width_ = 0;
    videoNv12Height_ = 0;
    videoNv12TextureCpuWritable_ = false;
  }

  bool EnsureVideoTextureForCpu(const UINT width, const UINT height, const bool cpuWritable) {
    if (!videoPipelineReady_ || device_ == nullptr || width == 0 || height == 0) {
      return false;
    }

    if (videoTexture_ != nullptr && videoSrv_ != nullptr &&
        videoTexWidth_ == width && videoTexHeight_ == height &&
        videoTexFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM &&
        videoTextureCpuWritable_ == cpuWritable) {
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
    texDesc.Usage = cpuWritable ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags =
        cpuWritable ? static_cast<UINT>(D3D11_CPU_ACCESS_WRITE) : 0U;

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
    videoTexFormat_ = DXGI_FORMAT_B8G8R8A8_UNORM;
    videoTextureCpuWritable_ = cpuWritable;
    return true;
  }

  bool EnsureVideoTextureForGpu(const UINT width, const UINT height, const DXGI_FORMAT format) {
    if (!videoPipelineReady_ || device_ == nullptr || width == 0 || height == 0) {
      return false;
    }
    if (format == DXGI_FORMAT_UNKNOWN) {
      return false;
    }

    if (videoTexture_ != nullptr && videoSrv_ != nullptr &&
        videoTexWidth_ == width && videoTexHeight_ == height &&
        videoTexFormat_ == format && !videoTextureCpuWritable_) {
      return true;
    }

    ReleaseVideoTexture();

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

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
    videoTexFormat_ = format;
    videoTextureCpuWritable_ = false;
    return true;
  }

  bool EnsureVideoTextureForExternalNv12(const frame_bridge::LatestFrame& frame) {
    if (!videoPipelineReady_ || device_ == nullptr || frame.width <= 0 || frame.height <= 0 ||
        frame.gpuTexture == nullptr || frame.gpuAuxTexture == nullptr) {
      return false;
    }

    auto* yTexture = frame.gpuTexture;
    auto* uvTexture = frame.gpuAuxTexture;
    const UINT width = static_cast<UINT>(frame.width);
    const UINT height = static_cast<UINT>(frame.height);

    if (HasNv12VideoTexture() && videoNv12Width_ == width && videoNv12Height_ == height &&
        videoNv12YTexture_ == yTexture && videoNv12UvTexture_ == uvTexture &&
        !videoNv12TextureCpuWritable_) {
      return true;
    }

    ReleaseVideoTexture();

    yTexture->AddRef();
    videoNv12YTexture_ = yTexture;
    D3D11_SHADER_RESOURCE_VIEW_DESC ySrvDesc{};
    ySrvDesc.Format = DXGI_FORMAT_R8_UNORM;
    ySrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ySrvDesc.Texture2D.MipLevels = 1;
    HRESULT hr = device_->CreateShaderResourceView(videoNv12YTexture_, &ySrvDesc, &videoNv12YSrv_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    uvTexture->AddRef();
    videoNv12UvTexture_ = uvTexture;
    D3D11_SHADER_RESOURCE_VIEW_DESC uvSrvDesc{};
    uvSrvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    uvSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    uvSrvDesc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(videoNv12UvTexture_, &uvSrvDesc, &videoNv12UvSrv_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    videoNv12Width_ = width;
    videoNv12Height_ = height;
    videoNv12TextureCpuWritable_ = false;
    return true;
  }

  bool EnsureVideoTextureForNv12(const UINT width, const UINT height, const bool cpuWritable) {
    if (!videoPipelineReady_ || device_ == nullptr || width == 0 || height == 0) {
      return false;
    }

    if (HasNv12VideoTexture() && videoNv12Width_ == width && videoNv12Height_ == height &&
        videoNv12TextureCpuWritable_ == cpuWritable) {
      return true;
    }

    ReleaseVideoTexture();

    D3D11_TEXTURE2D_DESC yDesc{};
    yDesc.Width = width;
    yDesc.Height = height;
    yDesc.MipLevels = 1;
    yDesc.ArraySize = 1;
    yDesc.Format = DXGI_FORMAT_R8_UNORM;
    yDesc.SampleDesc.Count = 1;
    yDesc.Usage = cpuWritable ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    yDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    yDesc.CPUAccessFlags =
        cpuWritable ? static_cast<UINT>(D3D11_CPU_ACCESS_WRITE) : 0U;

    HRESULT hr = device_->CreateTexture2D(&yDesc, nullptr, &videoNv12YTexture_);
    if (FAILED(hr)) {
      return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC ySrvDesc{};
    ySrvDesc.Format = yDesc.Format;
    ySrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ySrvDesc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(videoNv12YTexture_, &ySrvDesc, &videoNv12YSrv_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    D3D11_TEXTURE2D_DESC uvDesc{};
    uvDesc.Width = width / 2;
    uvDesc.Height = height / 2;
    uvDesc.MipLevels = 1;
    uvDesc.ArraySize = 1;
    uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    uvDesc.SampleDesc.Count = 1;
    uvDesc.Usage = cpuWritable ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    uvDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    uvDesc.CPUAccessFlags =
        cpuWritable ? static_cast<UINT>(D3D11_CPU_ACCESS_WRITE) : 0U;

    hr = device_->CreateTexture2D(&uvDesc, nullptr, &videoNv12UvTexture_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC uvSrvDesc{};
    uvSrvDesc.Format = uvDesc.Format;
    uvSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    uvSrvDesc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(videoNv12UvTexture_, &uvSrvDesc, &videoNv12UvSrv_);
    if (FAILED(hr)) {
      ReleaseVideoTexture();
      return false;
    }

    videoNv12Width_ = width;
    videoNv12Height_ = height;
    videoNv12TextureCpuWritable_ = cpuWritable;
    return true;
  }

  bool CopyGpuVideoFrame(const frame_bridge::LatestFrame& frame) {
    if (context_ == nullptr || videoTexture_ == nullptr || frame.gpuTexture == nullptr) {
      return false;
    }
    if (!frame.gpuBacked) {
      return false;
    }
    if (frame.width <= 0 || frame.height <= 0) {
      return false;
    }

    auto* sourceTexture = frame.gpuTexture;
    context_->CopySubresourceRegion(videoTexture_, 0, 0, 0, 0, sourceTexture,
                                    frame.gpuSubresourceIndex, nullptr);
    return true;
  }

  bool UploadVideoFrame(const frame_bridge::LatestFrame& frame,
                        const ResolvedUploadScalePlan& uploadScalePlan) {
    if (context_ == nullptr || videoTexture_ == nullptr || frame.rgbaData == nullptr) {
      return false;
    }

    if (frame.width <= 0 || frame.height <= 0 || frame.strideBytes <= 0) {
      return false;
    }

    const UINT targetWidth =
        uploadScalePlan.targetWidth > 0 ? uploadScalePlan.targetWidth : static_cast<UINT>(frame.width);
    const UINT targetHeight = uploadScalePlan.targetHeight > 0 ? uploadScalePlan.targetHeight
                                                               : static_cast<UINT>(frame.height);
    if (targetWidth == 0 || targetHeight == 0) {
      return false;
    }

    const std::size_t minBytes = static_cast<std::size_t>(frame.strideBytes) *
                                 static_cast<std::size_t>(frame.height);
    if (frame.rgbaDataBytes < minBytes) {
      return false;
    }

    const auto* src = frame.rgbaData;
    const UINT srcRowPitch = static_cast<UINT>(frame.strideBytes);
    if (!uploadScalePlan.scaled && !videoTextureCpuWritable_) {
      context_->UpdateSubresource(videoTexture_, 0, nullptr, src, srcRowPitch,
                                  srcRowPitch * static_cast<UINT>(frame.height));
      return true;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(videoTexture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) ||
        mapped.pData == nullptr || mapped.RowPitch == 0) {
      return false;
    }
    WP_ASSUME(mapped.RowPitch > 0);
    WP_ASSUME(frame.height > 0);

    auto* dst = static_cast<std::uint8_t*>(mapped.pData);
    const UINT rowCopyBytes =
        SelectRgbaUploadRowCopyBytes(static_cast<int>(targetWidth), srcRowPitch, mapped.RowPitch);
    if (!uploadScalePlan.scaled) {
      const std::size_t srcBytesTotal = static_cast<std::size_t>(srcRowPitch) *
                                        static_cast<std::size_t>(frame.height);
      const std::size_t dstBytesTotal = static_cast<std::size_t>(mapped.RowPitch) *
                                        static_cast<std::size_t>(targetHeight);
      if (ShouldCopyRowsAsSingleContiguousBlock(frame.height, srcRowPitch, mapped.RowPitch,
                                                rowCopyBytes)) {
        std::memcpy(dst, src, static_cast<std::size_t>(rowCopyBytes) *
                                  static_cast<std::size_t>(frame.height));
      } else {
        CopyLinearRows(std::span<const std::uint8_t>(src, srcBytesTotal), srcRowPitch,
                       std::span<std::uint8_t>(dst, dstBytesTotal), mapped.RowPitch, frame.height,
                       rowCopyBytes);
      }
    } else {
      const UINT writablePixels = rowCopyBytes / 4U;
      DownscaleRgbaNearest(src, srcRowPitch, frame.width, frame.height, dst, mapped.RowPitch,
                           std::min(targetWidth, writablePixels), targetHeight);
    }
    context_->Unmap(videoTexture_, 0);
    return true;
  }

  bool UploadVideoFrameNv12(const frame_bridge::LatestFrame& frame,
                            const ResolvedUploadScalePlan& uploadScalePlan) {
    if (context_ == nullptr || videoNv12YTexture_ == nullptr || videoNv12UvTexture_ == nullptr ||
        frame.pixelFormat != frame_bridge::PixelFormat::kNv12 ||
        frame.yPlaneData == nullptr || frame.uvPlaneData == nullptr) {
      return false;
    }

    if (frame.width <= 0 || frame.height <= 0 || frame.yPlaneStrideBytes <= 0 ||
        frame.uvPlaneStrideBytes <= 0) {
      return false;
    }

    const UINT targetWidth =
        uploadScalePlan.targetWidth > 0 ? uploadScalePlan.targetWidth : static_cast<UINT>(frame.width);
    const UINT targetHeight = uploadScalePlan.targetHeight > 0 ? uploadScalePlan.targetHeight
                                                               : static_cast<UINT>(frame.height);
    if (targetWidth == 0 || targetHeight == 0 || (targetWidth & 1U) != 0U ||
        (targetHeight & 1U) != 0U) {
      return false;
    }

    const int uvRows = frame.height / 2;
    const std::size_t minYBytes = static_cast<std::size_t>(frame.yPlaneStrideBytes) *
                                  static_cast<std::size_t>(frame.height);
    const std::size_t minUvBytes = static_cast<std::size_t>(frame.uvPlaneStrideBytes) *
                                   static_cast<std::size_t>(uvRows);
    if (frame.yPlaneBytes < minYBytes || frame.uvPlaneBytes < minUvBytes) {
      return false;
    }

    const auto* ySrc = frame.yPlaneData;
    const UINT ySrcRowPitch = static_cast<UINT>(frame.yPlaneStrideBytes);
    const auto* uvSrc = frame.uvPlaneData;
    const UINT uvSrcRowPitch = static_cast<UINT>(frame.uvPlaneStrideBytes);
    if (!uploadScalePlan.scaled && !videoNv12TextureCpuWritable_) {
      context_->UpdateSubresource(videoNv12YTexture_, 0, nullptr, ySrc, ySrcRowPitch,
                                  ySrcRowPitch * static_cast<UINT>(frame.height));
      context_->UpdateSubresource(videoNv12UvTexture_, 0, nullptr, uvSrc, uvSrcRowPitch,
                                  uvSrcRowPitch * static_cast<UINT>(uvRows));
      return true;
    }

    D3D11_MAPPED_SUBRESOURCE yMapped{};
    if (FAILED(context_->Map(videoNv12YTexture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &yMapped)) ||
        yMapped.pData == nullptr || yMapped.RowPitch == 0) {
      return false;
    }
    WP_ASSUME(yMapped.RowPitch > 0);
    WP_ASSUME(frame.height > 0);

    auto* yDst = static_cast<std::uint8_t*>(yMapped.pData);
    const UINT yCopyBytes =
        SelectNv12UploadRowCopyBytes(static_cast<int>(targetWidth), ySrcRowPitch, yMapped.RowPitch);
    if (!uploadScalePlan.scaled) {
      const std::size_t ySrcBytesTotal = static_cast<std::size_t>(ySrcRowPitch) *
                                         static_cast<std::size_t>(frame.height);
      const std::size_t yDstBytesTotal = static_cast<std::size_t>(yMapped.RowPitch) *
                                         static_cast<std::size_t>(targetHeight);
      if (ShouldCopyRowsAsSingleContiguousBlock(frame.height, ySrcRowPitch, yMapped.RowPitch,
                                                yCopyBytes)) {
        std::memcpy(yDst, ySrc, static_cast<std::size_t>(yCopyBytes) *
                                  static_cast<std::size_t>(frame.height));
      } else {
        CopyLinearRows(std::span<const std::uint8_t>(ySrc, ySrcBytesTotal), ySrcRowPitch,
                       std::span<std::uint8_t>(yDst, yDstBytesTotal), yMapped.RowPitch, frame.height,
                       yCopyBytes);
      }
    } else {
      DownscalePlaneNearest(ySrc, ySrcRowPitch, frame.width, frame.height, yDst, yMapped.RowPitch,
                            std::min(targetWidth, yCopyBytes), targetHeight);
    }
    context_->Unmap(videoNv12YTexture_, 0);

    D3D11_MAPPED_SUBRESOURCE uvMapped{};
    if (FAILED(context_->Map(videoNv12UvTexture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &uvMapped)) ||
        uvMapped.pData == nullptr || uvMapped.RowPitch == 0) {
      return false;
    }
    WP_ASSUME(uvMapped.RowPitch > 0);
    WP_ASSUME(uvRows >= 0);

    auto* uvDst = static_cast<std::uint8_t*>(uvMapped.pData);
    const UINT uvCopyBytes = SelectNv12UploadRowCopyBytes(static_cast<int>(targetWidth),
                                                          uvSrcRowPitch, uvMapped.RowPitch);
    if (!uploadScalePlan.scaled) {
      const std::size_t uvSrcBytesTotal =
          static_cast<std::size_t>(uvSrcRowPitch) * static_cast<std::size_t>(uvRows);
      const std::size_t uvDstBytesTotal =
          static_cast<std::size_t>(uvMapped.RowPitch) * static_cast<std::size_t>(targetHeight / 2U);
      if (ShouldCopyRowsAsSingleContiguousBlock(uvRows, uvSrcRowPitch, uvMapped.RowPitch,
                                                uvCopyBytes)) {
        std::memcpy(uvDst, uvSrc, static_cast<std::size_t>(uvCopyBytes) *
                                    static_cast<std::size_t>(uvRows));
      } else {
        CopyLinearRows(std::span<const std::uint8_t>(uvSrc, uvSrcBytesTotal), uvSrcRowPitch,
                       std::span<std::uint8_t>(uvDst, uvDstBytesTotal), uvMapped.RowPitch, uvRows,
                       uvCopyBytes);
      }
    } else {
      const UINT targetChromaCols = std::min(targetWidth / 2U, uvCopyBytes / 2U);
      DownscaleInterleavedUvNearest(uvSrc, uvSrcRowPitch, frame.width / 2, uvRows, uvDst,
                                    uvMapped.RowPitch, targetChromaCols, targetHeight / 2U);
    }
    context_->Unmap(videoNv12UvTexture_, 0);
    return true;
  }

  bool DrawVideoResources(ID3D11PixelShader* pixelShader, ID3D11ShaderResourceView* const* srvs,
                          const UINT srvCount) {
    if (!videoPipelineReady_ || context_ == nullptr || renderTargetView_ == nullptr ||
        pixelShader == nullptr || srvs == nullptr || srvCount == 0 || videoSampler_ == nullptr ||
        videoInputLayout_ == nullptr || videoVertexBuffer_ == nullptr ||
        videoVertexShader_ == nullptr) {
      return false;
    }

    if (renderViewportWidth_ == 0 || renderViewportHeight_ == 0) {
      if (!RefreshRenderViewportCacheFromWindow()) {
        return false;
      }
    }

    context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);

    const UINT stride = sizeof(VideoVertex);
    const UINT offset = 0;
    context_->IASetInputLayout(videoInputLayout_);
    context_->IASetVertexBuffers(0, 1, &videoVertexBuffer_, &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(videoVertexShader_, nullptr, 0);
    context_->PSSetShader(pixelShader, nullptr, 0);
    context_->PSSetSamplers(0, 1, &videoSampler_);
    context_->PSSetShaderResources(0, srvCount, srvs);

    if (monitorViewports_.empty()) {
      RebuildMonitorViewports();
    }

    bool drewAnyViewport = false;
    for (const RenderViewport& viewportRect : monitorViewports_) {
      if (viewportRect.width <= 0 || viewportRect.height <= 0) {
        continue;
      }

      D3D11_VIEWPORT viewport{};
      viewport.TopLeftX = static_cast<float>(viewportRect.left);
      viewport.TopLeftY = static_cast<float>(viewportRect.top);
      viewport.Width = static_cast<float>(viewportRect.width);
      viewport.Height = static_cast<float>(viewportRect.height);
      viewport.MinDepth = 0.0f;
      viewport.MaxDepth = 1.0f;
      context_->RSSetViewports(1, &viewport);
      // 每个显示器视口都重复绘制同一帧，避免多屏被拼成一个跨屏画面。
      context_->Draw(4, 0);
      drewAnyViewport = true;
    }
    if (!drewAnyViewport) {
      D3D11_VIEWPORT fallbackViewport{};
      fallbackViewport.Width = static_cast<float>(renderViewportWidth_);
      fallbackViewport.Height = static_cast<float>(renderViewportHeight_);
      fallbackViewport.MinDepth = 0.0f;
      fallbackViewport.MaxDepth = 1.0f;
      context_->RSSetViewports(1, &fallbackViewport);
      context_->Draw(4, 0);
    }

    ID3D11ShaderResourceView* nullSrvs[2] = {nullptr, nullptr};
    context_->PSSetShaderResources(0, srvCount, nullSrvs);
    return true;
  }

  bool DrawVideoTexture() {
    return DrawVideoResources(videoPixelShader_, &videoSrv_, 1);
  }

  bool DrawVideoNv12Texture() {
    ID3D11ShaderResourceView* nv12Srvs[] = {videoNv12YSrv_, videoNv12UvSrv_};
    return DrawVideoResources(videoNv12PixelShader_, nv12Srvs, 2);
  }

  bool DrawCachedVideoTexture() {
    if (HasNv12VideoTexture()) {
      return DrawVideoNv12Texture();
    }
    if (HasRgbaVideoTexture()) {
      return DrawVideoTexture();
    }
    return false;
  }

  void ResizeSwapChain(UINT width, UINT height) {
    if (swapChain_ == nullptr || context_ == nullptr) {
      return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    SafeRelease(&renderTargetView_);
    if (SUCCEEDED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
      UpdateRenderViewportCache(width, height);
      CreateRenderTargetView();
      return;
    }
    // 回退：ResizeBuffers 失败时尝试重建 RTV，避免渲染链路永久失效。
    CreateRenderTargetView();
  }

  void ReleaseD3D() {
    d3d11_interop::ClearSharedDevice(device_);
    swapChainOccluded_ = false;
    nextOcclusionProbeAtTick_ = 0;
    renderViewportWidth_ = 0;
    renderViewportHeight_ = 0;
    monitorViewports_.clear();
    lastPostPresentTrimAtTick_ = 0;
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
  ULONGLONG lastPostPresentTrimAtTick_ = 0;

  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  IDXGISwapChain1* swapChain_ = nullptr;
  IDXGISwapChain2* swapChain2_ = nullptr;
  static constexpr ULONGLONG kOcclusionProbeIntervalMs = 250;
  bool swapChainOccluded_ = false;
  ULONGLONG nextOcclusionProbeAtTick_ = 0;
  ID3D11RenderTargetView* renderTargetView_ = nullptr;
  UINT renderViewportWidth_ = 0;
  UINT renderViewportHeight_ = 0;
  std::vector<RenderViewport> monitorViewports_;

  bool videoPipelineReady_ = false;
  ID3D11VertexShader* videoVertexShader_ = nullptr;
  ID3D11PixelShader* videoPixelShader_ = nullptr;
  ID3D11PixelShader* videoNv12PixelShader_ = nullptr;
  ID3D11InputLayout* videoInputLayout_ = nullptr;
  ID3D11Buffer* videoVertexBuffer_ = nullptr;
  ID3D11SamplerState* videoSampler_ = nullptr;
  ID3D11Texture2D* videoTexture_ = nullptr;
  ID3D11ShaderResourceView* videoSrv_ = nullptr;
  UINT videoTexWidth_ = 0;
  UINT videoTexHeight_ = 0;
  DXGI_FORMAT videoTexFormat_ = DXGI_FORMAT_UNKNOWN;
  bool videoTextureCpuWritable_ = false;
  ID3D11Texture2D* videoNv12YTexture_ = nullptr;
  ID3D11Texture2D* videoNv12UvTexture_ = nullptr;
  ID3D11ShaderResourceView* videoNv12YSrv_ = nullptr;
  ID3D11ShaderResourceView* videoNv12UvSrv_ = nullptr;
  UINT videoNv12Width_ = 0;
  UINT videoNv12Height_ = 0;
  bool videoNv12TextureCpuWritable_ = false;
  std::uint64_t lastVideoSequence_ = 0;
};

}  // namespace

std::unique_ptr<IWallpaperHost> CreateWallpaperHost() {
  return std::make_unique<WallpaperHostWin>();
}

}  // namespace wallpaper
