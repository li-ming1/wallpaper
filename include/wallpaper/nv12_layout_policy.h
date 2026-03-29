#pragma once

#include <cstddef>
#include <cstdint>

namespace wallpaper {

struct Nv12Layout final {
  std::size_t yPlaneOffsetBytes = 0;
  std::size_t yPlaneBytes = 0;
  std::size_t uvPlaneOffsetBytes = 0;
  std::size_t uvPlaneBytes = 0;
};

// 根据 NV12 buffer 的 pitch / 可见高度 / 总长度推导 Y 与 UV 平面布局。
// 当底层解码器按更高的对齐高度分配 buffer 时，不能假设 UV 紧跟 visibleHeight * pitch。
[[nodiscard]] Nv12Layout ComputeNv12Layout(std::uint32_t frameHeight, std::uint32_t pitch,
                                           std::size_t totalBytes) noexcept;

}  // namespace wallpaper
