#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wallpaper {

struct CompactCpuFrameBuffer final {
  int width = 0;
  int height = 0;
  int primaryStrideBytes = 0;
  int secondaryStrideBytes = 0;
  std::size_t primaryPlaneOffsetBytes = 0;
  std::size_t secondaryPlaneOffsetBytes = 0;
  std::vector<std::uint8_t> bytes;
};

[[nodiscard]] bool TryDownscaleRgbaFrameNearest(const std::uint8_t* srcData, int srcWidth,
                                                int srcHeight, int srcStrideBytes,
                                                int targetWidth, int targetHeight,
                                                CompactCpuFrameBuffer* outBuffer) noexcept;

[[nodiscard]] bool TryDownscaleNv12FrameNearest(const std::uint8_t* srcYPlaneData,
                                                int srcYStrideBytes,
                                                const std::uint8_t* srcUvPlaneData,
                                                int srcUvStrideBytes, int srcWidth, int srcHeight,
                                                int targetWidth, int targetHeight,
                                                CompactCpuFrameBuffer* outBuffer) noexcept;

}  // namespace wallpaper
