#pragma once

#include <cstdint>

namespace wallpaper {

// RGBA 上传按有效像素宽度裁剪行拷贝字节，避免复制 stride 尾部填充字节。
[[nodiscard]] std::uint32_t SelectRgbaUploadRowCopyBytes(int targetWidth,
                                                         std::uint32_t srcRowPitch,
                                                         std::uint32_t dstRowPitch) noexcept;

// NV12 上传按有效像素宽度裁剪行拷贝字节，避免复制 stride 尾部填充字节。
[[nodiscard]] std::uint32_t SelectNv12UploadRowCopyBytes(int targetWidth,
                                                         std::uint32_t srcRowPitch,
                                                         std::uint32_t dstRowPitch) noexcept;

// 当源/目标行跨度与有效拷贝字节完全一致时，可用单次 memcpy 覆盖整平面。
[[nodiscard]] bool ShouldCopyRowsAsSingleContiguousBlock(int rowCount,
                                                         std::uint32_t srcRowPitch,
                                                         std::uint32_t dstRowPitch,
                                                         std::uint32_t rowCopyBytes) noexcept;

}  // namespace wallpaper
