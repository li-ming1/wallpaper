#include "wallpaper/cpu_frame_downscale.h"

#include <cstring>

namespace wallpaper {
namespace {

[[nodiscard]] bool IsValidScaleRequest(const std::uint8_t* const srcData, const int srcWidth,
                                       const int srcHeight, const int srcStrideBytes,
                                       const int targetWidth, const int targetHeight,
                                       CompactCpuFrameBuffer* const outBuffer) noexcept {
  return srcData != nullptr && srcWidth > 0 && srcHeight > 0 && srcStrideBytes > 0 &&
         targetWidth > 0 && targetHeight > 0 && outBuffer != nullptr;
}

}  // namespace

bool TryDownscaleRgbaFrameNearest(const std::uint8_t* const srcData, const int srcWidth,
                                  const int srcHeight, const int srcStrideBytes,
                                  const int targetWidth, const int targetHeight,
                                  CompactCpuFrameBuffer* const outBuffer) noexcept {
  if (!IsValidScaleRequest(srcData, srcWidth, srcHeight, srcStrideBytes, targetWidth, targetHeight,
                           outBuffer)) {
    return false;
  }

  const std::size_t targetStrideBytes = static_cast<std::size_t>(targetWidth) * 4U;
  const std::size_t requiredBytes = targetStrideBytes * static_cast<std::size_t>(targetHeight);
  outBuffer->width = targetWidth;
  outBuffer->height = targetHeight;
  outBuffer->primaryStrideBytes = static_cast<int>(targetStrideBytes);
  outBuffer->secondaryStrideBytes = 0;
  outBuffer->primaryPlaneOffsetBytes = 0;
  outBuffer->secondaryPlaneOffsetBytes = 0;
  outBuffer->bytes.assign(requiredBytes, 0U);

  // CPU fallback 在发布到 frame_bridge 前就压成紧凑小帧，避免运行期继续持有大样本。
  for (int y = 0; y < targetHeight; ++y) {
    const int srcY = (y * srcHeight) / targetHeight;
    const auto* const srcRow =
        srcData + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(srcStrideBytes);
    auto* const dstRow = outBuffer->bytes.data() + static_cast<std::size_t>(y) * targetStrideBytes;
    for (int x = 0; x < targetWidth; ++x) {
      const int srcX = (x * srcWidth) / targetWidth;
      const auto* const srcPixel = srcRow + static_cast<std::size_t>(srcX) * 4U;
      auto* const dstPixel = dstRow + static_cast<std::size_t>(x) * 4U;
      std::memcpy(dstPixel, srcPixel, 4U);
    }
  }
  return true;
}

bool TryDownscaleNv12FrameNearest(const std::uint8_t* const srcYPlaneData,
                                  const int srcYStrideBytes,
                                  const std::uint8_t* const srcUvPlaneData,
                                  const int srcUvStrideBytes, const int srcWidth,
                                  const int srcHeight, const int targetWidth,
                                  const int targetHeight,
                                  CompactCpuFrameBuffer* const outBuffer) noexcept {
  if (!IsValidScaleRequest(srcYPlaneData, srcWidth, srcHeight, srcYStrideBytes, targetWidth,
                           targetHeight, outBuffer) ||
      srcUvPlaneData == nullptr || srcUvStrideBytes <= 0 || srcWidth % 2 != 0 ||
      srcHeight % 2 != 0 || targetWidth % 2 != 0 || targetHeight % 2 != 0) {
    return false;
  }

  const std::size_t yStrideBytes = static_cast<std::size_t>(targetWidth);
  const std::size_t uvStrideBytes = static_cast<std::size_t>(targetWidth);
  const std::size_t yPlaneBytes = yStrideBytes * static_cast<std::size_t>(targetHeight);
  const std::size_t uvPlaneBytes =
      uvStrideBytes * static_cast<std::size_t>(targetHeight / 2);
  outBuffer->width = targetWidth;
  outBuffer->height = targetHeight;
  outBuffer->primaryStrideBytes = static_cast<int>(yStrideBytes);
  outBuffer->secondaryStrideBytes = static_cast<int>(uvStrideBytes);
  outBuffer->primaryPlaneOffsetBytes = 0;
  outBuffer->secondaryPlaneOffsetBytes = yPlaneBytes;
  outBuffer->bytes.assign(yPlaneBytes + uvPlaneBytes, 0U);

  for (int y = 0; y < targetHeight; ++y) {
    const int srcY = (y * srcHeight) / targetHeight;
    const auto* const srcRow =
        srcYPlaneData + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(srcYStrideBytes);
    auto* const dstRow = outBuffer->bytes.data() + static_cast<std::size_t>(y) * yStrideBytes;
    for (int x = 0; x < targetWidth; ++x) {
      const int srcX = (x * srcWidth) / targetWidth;
      dstRow[x] = srcRow[srcX];
    }
  }

  const int srcUvWidthSamples = srcWidth / 2;
  const int srcUvHeightSamples = srcHeight / 2;
  const int targetUvWidthSamples = targetWidth / 2;
  const int targetUvHeightSamples = targetHeight / 2;
  auto* const dstUvBase = outBuffer->bytes.data() + yPlaneBytes;
  for (int y = 0; y < targetUvHeightSamples; ++y) {
    const int srcY = (y * srcUvHeightSamples) / targetUvHeightSamples;
    const auto* const srcRow = srcUvPlaneData +
                               static_cast<std::size_t>(srcY) *
                                   static_cast<std::size_t>(srcUvStrideBytes);
    auto* const dstRow = dstUvBase + static_cast<std::size_t>(y) * uvStrideBytes;
    for (int x = 0; x < targetUvWidthSamples; ++x) {
      const int srcX = (x * srcUvWidthSamples) / targetUvWidthSamples;
      const auto* const srcPixel = srcRow + static_cast<std::size_t>(srcX) * 2U;
      auto* const dstPixel = dstRow + static_cast<std::size_t>(x) * 2U;
      dstPixel[0] = srcPixel[0];
      dstPixel[1] = srcPixel[1];
    }
  }

  return true;
}

}  // namespace wallpaper
