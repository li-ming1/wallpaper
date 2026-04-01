#pragma once

#include <cstddef>

#include "wallpaper/interfaces.h"

namespace wallpaper {

// 全画质 CPU fallback 且不做上传缩放时，优先使用 DEFAULT 纹理 + UpdateSubresource，
// 目标是减少 D3D dynamic texture 触达的常驻页。
[[nodiscard]] bool ShouldUseDefaultTextureUpload(DecodePath decodePath,
                                                 std::size_t decodeOutputPixels,
                                                 bool scaledUpload) noexcept;

// 全画质 NV12 CPU fallback 且不做上传缩放时，允许 decode 侧直接提升为共享 GPU 纹理桥，
// 以便尽快释放 MF CPU sample。
[[nodiscard]] bool ShouldUseSharedGpuNv12Bridge(DecodePath decodePath,
                                                std::size_t decodeOutputPixels,
                                                bool scaledUpload) noexcept;

}  // namespace wallpaper
