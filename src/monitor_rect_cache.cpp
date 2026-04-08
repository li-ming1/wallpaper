#include "wallpaper/monitor_rect_cache.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace wallpaper {
namespace {

constexpr std::chrono::seconds kRefreshInterval{2};

struct MonitorRectCacheState final {
  DisplayRect virtualDesktop{};
  DisplayRectPlan monitors{};
  std::chrono::steady_clock::time_point cachedAt{};
};

std::atomic<std::shared_ptr<const MonitorRectCacheState>> g_monitorCacheState{};
std::mutex g_monitorCacheWriteMu;

[[nodiscard]] bool IsSameDisplayRect(const DisplayRect& lhs, const DisplayRect& rhs) noexcept {
  return lhs.left == rhs.left && lhs.top == rhs.top && lhs.right == rhs.right &&
         lhs.bottom == rhs.bottom;
}

[[nodiscard]] DisplayRect QueryVirtualDesktopRect() noexcept {
  DisplayRect rect;
  rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  return rect;
}

BOOL CALLBACK CollectMonitorRect(HMONITOR, HDC, LPRECT monitorRect, LPARAM userData) {
  if (monitorRect == nullptr || userData == 0) {
    return TRUE;
  }
  auto* monitors = reinterpret_cast<DisplayRectPlan*>(userData);
  const DisplayRect rect{
      monitorRect->left,
      monitorRect->top,
      monitorRect->right,
      monitorRect->bottom,
  };
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return TRUE;
  }
  return monitors->PushBack(rect) ? TRUE : FALSE;
}

[[nodiscard]] DisplayRectPlan EnumerateMonitorRects() {
  DisplayRectPlan monitors;
  EnumDisplayMonitors(nullptr, nullptr, CollectMonitorRect, reinterpret_cast<LPARAM>(&monitors));
  return monitors;
}

[[nodiscard]] bool IsFreshCacheState(const std::shared_ptr<const MonitorRectCacheState>& state,
                                     const DisplayRect& virtualDesktop,
                                     const std::chrono::steady_clock::time_point now) noexcept {
  return state != nullptr && IsSameDisplayRect(state->virtualDesktop, virtualDesktop) &&
         (now - state->cachedAt) < kRefreshInterval;
}

[[nodiscard]] bool TryReadFreshCacheSnapshot(
    const DisplayRect& virtualDesktop, const std::chrono::steady_clock::time_point now,
    MonitorRectSnapshot* const outSnapshot) {
  if (outSnapshot == nullptr) {
    return false;
  }
  const std::shared_ptr<const MonitorRectCacheState> state =
      g_monitorCacheState.load(std::memory_order_acquire);
  if (!IsFreshCacheState(state, virtualDesktop, now)) {
    return false;
  }
  *outSnapshot = MonitorRectSnapshot{state->virtualDesktop, state->monitors};
  return true;
}

}  // namespace

MonitorRectSnapshot QueryMonitorRectSnapshotCached() {
  const DisplayRect virtualDesktop = QueryVirtualDesktopRect();
  const auto now = std::chrono::steady_clock::now();
  MonitorRectSnapshot cached;
  if (TryReadFreshCacheSnapshot(virtualDesktop, now, &cached)) {
    return cached;
  }

  DisplayRectPlan monitors = EnumerateMonitorRects();
  if (monitors.Empty()) {
    (void)monitors.PushBack(virtualDesktop);
  }

  std::lock_guard<std::mutex> lock(g_monitorCacheWriteMu);
  if (TryReadFreshCacheSnapshot(virtualDesktop, now, &cached)) {
    return cached;
  }
  auto nextState = std::make_shared<MonitorRectCacheState>();
  nextState->virtualDesktop = virtualDesktop;
  nextState->monitors = std::move(monitors);
  nextState->cachedAt = now;
  g_monitorCacheState.store(nextState, std::memory_order_release);
  return MonitorRectSnapshot{nextState->virtualDesktop, nextState->monitors};
}

void InvalidateMonitorRectSnapshotCache() noexcept {
  std::lock_guard<std::mutex> lock(g_monitorCacheWriteMu);
  g_monitorCacheState.store(std::shared_ptr<const MonitorRectCacheState>{},
                            std::memory_order_release);
}

}  // namespace wallpaper
