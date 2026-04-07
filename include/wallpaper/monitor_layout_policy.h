#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace wallpaper {

struct DisplayRect final {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

struct RenderViewport final {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
};

struct DisplaySize final {
  int width = 0;
  int height = 0;
};

template <typename T, std::size_t MaxCount>
struct FixedMonitorLayoutPlan {
  std::array<T, MaxCount> entries{};
  std::size_t count = 0;

  [[nodiscard]] std::size_t Size() const noexcept { return count; }
  [[nodiscard]] std::size_t Capacity() const noexcept { return entries.size(); }
  [[nodiscard]] bool Empty() const noexcept { return count == 0; }
  [[nodiscard]] bool Full() const noexcept { return count >= entries.size(); }

  void Clear() noexcept { count = 0; }

  [[nodiscard]] bool PushBack(const T& value) noexcept {
    if (count >= entries.size()) {
      return false;
    }
    entries[count] = value;
    ++count;
    return true;
  }

  [[nodiscard]] const T& operator[](const std::size_t index) const noexcept {
    return entries[index];
  }

  [[nodiscard]] T& operator[](const std::size_t index) noexcept { return entries[index]; }

  [[nodiscard]] std::span<const T> Items() const noexcept { return {entries.data(), count}; }
  [[nodiscard]] std::span<T> Items() noexcept { return {entries.data(), count}; }
};

using DisplayRectPlan = FixedMonitorLayoutPlan<DisplayRect, 16>;
using RenderViewportPlan = FixedMonitorLayoutPlan<RenderViewport, 16>;

[[nodiscard]] RenderViewportPlan BuildRenderMonitorViewports(
    const DisplayRect& virtualDesktop, const DisplayRectPlan& monitors) noexcept;

[[nodiscard]] RenderViewportPlan BuildScaledRenderMonitorViewports(
    const DisplayRect& virtualDesktop, const DisplayRectPlan& monitors,
    int renderTargetWidth, int renderTargetHeight) noexcept;

[[nodiscard]] DisplaySize SelectRepeatedFrameRenderSize(
    const DisplayRect& virtualDesktop, const DisplayRectPlan& monitors) noexcept;

}  // namespace wallpaper
