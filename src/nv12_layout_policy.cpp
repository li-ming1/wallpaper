#include "wallpaper/nv12_layout_policy.h"

#include <algorithm>

namespace wallpaper {

Nv12Layout ComputeNv12Layout(const std::uint32_t frameHeight, const std::uint32_t pitch,
                             const std::size_t totalBytes) noexcept {
  Nv12Layout layout;
  if (frameHeight == 0 || pitch == 0 || totalBytes == 0) {
    return layout;
  }

  const std::size_t visibleYBytes =
      static_cast<std::size_t>(frameHeight) * static_cast<std::size_t>(pitch);
  if (totalBytes <= visibleYBytes) {
    return layout;
  }

  const std::size_t totalRows = totalBytes / static_cast<std::size_t>(pitch);
  if (totalRows == 0) {
    return layout;
  }

  // NV12 总行数 = 对齐后的 Y 行数 + 对齐后的 UV 行数 = alignedY * 3 / 2。
  std::size_t alignedYRows = (totalRows * 2U) / 3U;
  alignedYRows = std::max<std::size_t>(alignedYRows, frameHeight);
  const std::size_t uvOffsetBytes = alignedYRows * static_cast<std::size_t>(pitch);
  if (uvOffsetBytes >= totalBytes) {
    return layout;
  }

  layout.yPlaneBytes = visibleYBytes;
  layout.uvPlaneOffsetBytes = uvOffsetBytes;
  layout.uvPlaneBytes = totalBytes - uvOffsetBytes;
  return layout;
}

}  // namespace wallpaper
