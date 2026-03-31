#include "wallpaper/upload_copy_policy.h"

#include <algorithm>

namespace wallpaper {

std::uint32_t SelectRgbaUploadRowCopyBytes(const int targetWidth, const std::uint32_t srcRowPitch,
                                           const std::uint32_t dstRowPitch) noexcept {
  if (targetWidth <= 0 || srcRowPitch == 0 || dstRowPitch == 0) {
    return 0;
  }
  const std::uint64_t activeRowBytes = static_cast<std::uint64_t>(targetWidth) * 4ULL;
  const auto clampedActiveRowBytes =
      static_cast<std::uint32_t>(std::min<std::uint64_t>(activeRowBytes, 0xFFFFFFFFULL));
  return std::min({clampedActiveRowBytes, srcRowPitch, dstRowPitch});
}

std::uint32_t SelectNv12UploadRowCopyBytes(const int targetWidth, const std::uint32_t srcRowPitch,
                                           const std::uint32_t dstRowPitch) noexcept {
  if (targetWidth <= 0 || srcRowPitch == 0 || dstRowPitch == 0) {
    return 0;
  }
  const auto activeRowBytes = static_cast<std::uint32_t>(targetWidth);
  return std::min({activeRowBytes, srcRowPitch, dstRowPitch});
}

bool ShouldCopyRowsAsSingleContiguousBlock(const int rowCount, const std::uint32_t srcRowPitch,
                                           const std::uint32_t dstRowPitch,
                                           const std::uint32_t rowCopyBytes) noexcept {
  if (rowCount <= 0 || rowCopyBytes == 0) {
    return false;
  }
  return srcRowPitch == dstRowPitch && srcRowPitch == rowCopyBytes;
}

}  // namespace wallpaper
