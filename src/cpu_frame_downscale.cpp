#include "wallpaper/cpu_frame_downscale.h"
#include "wallpaper/nearest_scale_stepper.h"

#include <immintrin.h>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace wallpaper {
namespace {

[[nodiscard]] bool IsValidScaleRequest(const std::uint8_t* const srcData, const int srcWidth,
                                        const int srcHeight, const int srcStrideBytes,
                                        const int targetWidth, const int targetHeight,
                                        CpuFrameBufferPool* const pool,
                                        CompactCpuFrameBuffer* const outBuffer) noexcept {
  return srcData != nullptr && srcWidth > 0 && srcHeight > 0 && srcStrideBytes > 0 &&
         targetWidth > 0 && targetHeight > 0 && pool != nullptr && outBuffer != nullptr;
}

[[nodiscard]] bool AcquireOutputBuffer(CpuFrameBufferPool* const pool,
                                       const std::size_t requiredBytes,
                                       CompactCpuFrameBuffer* const outBuffer) noexcept {
  if (pool == nullptr || outBuffer == nullptr || requiredBytes == 0) {
    return false;
  }

  CpuFrameBufferLease lease = pool->Acquire(requiredBytes);
  if (lease.data == nullptr || lease.capacityBytes < requiredBytes || lease.holder == nullptr) {
    return false;
  }

  outBuffer->data = lease.data;
  outBuffer->dataBytes = requiredBytes;
  outBuffer->holder = std::move(lease.holder);
  return true;
}

void CopyBytesSimd128(const std::uint8_t* const src, std::uint8_t* const dst,
                      const std::size_t bytes) noexcept {
  if (src == nullptr || dst == nullptr || bytes == 0) {
    return;
  }

  std::size_t offset = 0;
  for (; offset + 16U <= bytes; offset += 16U) {
    const __m128i pack =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + offset));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + offset), pack);
  }
  if (offset < bytes) {
    std::memcpy(dst + offset, src + offset, bytes - offset);
  }
}

struct NearestIndexTableCacheEntry final {
  int srcExtent = 0;
  int targetExtent = 0;
  std::vector<int> indices;
  std::uint64_t useStamp = 0;
  bool occupied = false;
};

// 下采样尺寸组合在运行期高度重复，线程内缓存可把重复建表从 O(targetExtent) 降到 O(1) 命中。
class NearestIndexTableCache final {
 public:
  [[nodiscard]] const std::vector<int>& Get(const int srcExtent, const int targetExtent) {
    if (srcExtent <= 0 || targetExtent <= 0) {
      return empty_;
    }

    const std::size_t hitIndex = FindHitIndex(srcExtent, targetExtent);
    if (hitIndex != kInvalidIndex) {
      NearestIndexTableCacheEntry& hit = entries_[hitIndex];
      hit.useStamp = ++nextUseStamp_;
      return hit.indices;
    }

    const std::size_t insertIndex = FindInsertIndex();
    if (insertIndex == kInvalidIndex) {
      return empty_;
    }
    NearestIndexTableCacheEntry& entry = entries_[insertIndex];
    entry.indices.resize(static_cast<std::size_t>(targetExtent));
    NearestScaleStepper stepper(srcExtent, targetExtent);
    for (int index = 0; index < targetExtent; ++index) {
      entry.indices[static_cast<std::size_t>(index)] = stepper.CurrentSourceIndex();
      stepper.Advance();
    }
    entry.srcExtent = srcExtent;
    entry.targetExtent = targetExtent;
    entry.useStamp = ++nextUseStamp_;
    entry.occupied = true;
    return entry.indices;
  }

 private:
  static constexpr std::size_t kCacheSlotCount = 8;
  static constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

  [[nodiscard]] std::size_t FindHitIndex(const int srcExtent, const int targetExtent) const noexcept {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const NearestIndexTableCacheEntry& entry = entries_[index];
      if (entry.occupied && entry.srcExtent == srcExtent && entry.targetExtent == targetExtent) {
        return index;
      }
    }
    return kInvalidIndex;
  }

  [[nodiscard]] std::size_t FindInsertIndex() const noexcept {
    std::size_t insertIndex = kInvalidIndex;
    std::uint64_t oldestStamp = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const NearestIndexTableCacheEntry& entry = entries_[index];
      if (!entry.occupied) {
        return index;
      }
      if (entry.useStamp < oldestStamp) {
        oldestStamp = entry.useStamp;
        insertIndex = index;
      }
    }
    return insertIndex;
  }

  std::array<NearestIndexTableCacheEntry, kCacheSlotCount> entries_{};
  std::uint64_t nextUseStamp_ = 0;
  std::vector<int> empty_{};
};

struct ByteOffsetTableCacheEntry final {
  int srcExtent = 0;
  int targetExtent = 0;
  std::size_t bytesPerSample = 0;
  std::vector<std::size_t> offsets;
  std::uint64_t useStamp = 0;
  bool occupied = false;
};

// 偏移表和 index 表一样会在少量分辨率组合间高频复用，线程内缓存可避免每帧重建 vector。
class ByteOffsetTableCache final {
 public:
  [[nodiscard]] const std::vector<std::size_t>& Get(const int srcExtent, const int targetExtent,
                                                    const std::size_t bytesPerSample,
                                                    const std::vector<int>& sourceIndices) {
    if (srcExtent <= 0 || targetExtent <= 0 || bytesPerSample == 0 ||
        sourceIndices.size() != static_cast<std::size_t>(targetExtent)) {
      return empty_;
    }

    const std::size_t hitIndex = FindHitIndex(srcExtent, targetExtent, bytesPerSample);
    if (hitIndex != kInvalidIndex) {
      ByteOffsetTableCacheEntry& hit = entries_[hitIndex];
      hit.useStamp = ++nextUseStamp_;
      return hit.offsets;
    }

    const std::size_t insertIndex = FindInsertIndex();
    if (insertIndex == kInvalidIndex) {
      return empty_;
    }

    ByteOffsetTableCacheEntry& entry = entries_[insertIndex];
    entry.offsets.resize(sourceIndices.size());
    for (std::size_t index = 0; index < sourceIndices.size(); ++index) {
      entry.offsets[index] = static_cast<std::size_t>(sourceIndices[index]) * bytesPerSample;
    }
    entry.srcExtent = srcExtent;
    entry.targetExtent = targetExtent;
    entry.bytesPerSample = bytesPerSample;
    entry.useStamp = ++nextUseStamp_;
    entry.occupied = true;
    return entry.offsets;
  }

 private:
  static constexpr std::size_t kCacheSlotCount = 8;
  static constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

  [[nodiscard]] std::size_t FindHitIndex(const int srcExtent, const int targetExtent,
                                         const std::size_t bytesPerSample) const noexcept {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const ByteOffsetTableCacheEntry& entry = entries_[index];
      if (entry.occupied && entry.srcExtent == srcExtent &&
          entry.targetExtent == targetExtent && entry.bytesPerSample == bytesPerSample) {
        return index;
      }
    }
    return kInvalidIndex;
  }

  [[nodiscard]] std::size_t FindInsertIndex() const noexcept {
    std::size_t insertIndex = kInvalidIndex;
    std::uint64_t oldestStamp = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const ByteOffsetTableCacheEntry& entry = entries_[index];
      if (!entry.occupied) {
        return index;
      }
      if (entry.useStamp < oldestStamp) {
        oldestStamp = entry.useStamp;
        insertIndex = index;
      }
    }
    return insertIndex;
  }

  std::array<ByteOffsetTableCacheEntry, kCacheSlotCount> entries_{};
  std::uint64_t nextUseStamp_ = 0;
  std::vector<std::size_t> empty_{};
};

[[nodiscard]] const std::vector<int>& BuildNearestSourceIndexTable(const int srcExtent,
                                                                   const int targetExtent) {
  thread_local NearestIndexTableCache cache;
  return cache.Get(srcExtent, targetExtent);
}

[[nodiscard]] const std::vector<std::size_t>& BuildSampleByteOffsetTable(
    const int srcExtent, const int targetExtent, const std::size_t bytesPerSample,
    const std::vector<int>& sourceIndices) {
  thread_local ByteOffsetTableCache cache;
  return cache.Get(srcExtent, targetExtent, bytesPerSample, sourceIndices);
}

[[nodiscard]] const std::vector<std::size_t>& BuildRgbaSourceByteOffsetTable(
    const int srcWidth, const int targetWidth, const std::vector<int>& xIndices) {
  return BuildSampleByteOffsetTable(srcWidth, targetWidth, 4U, xIndices);
}

[[nodiscard]] const std::vector<std::size_t>& BuildInterleavedUvSourceByteOffsetTable(
    const int srcUvWidthSamples, const int targetUvWidthSamples,
    const std::vector<int>& uvXIndices) {
  return BuildSampleByteOffsetTable(srcUvWidthSamples, targetUvWidthSamples, 2U, uvXIndices);
}

}  // namespace

bool TryDownscaleRgbaFrameNearest(const std::uint8_t* const srcData, const int srcWidth,
                                  const int srcHeight, const int srcStrideBytes,
                                  const int targetWidth, const int targetHeight,
                                  CpuFrameBufferPool* const pool,
                                  CompactCpuFrameBuffer* const outBuffer) noexcept {
  if (!IsValidScaleRequest(srcData, srcWidth, srcHeight, srcStrideBytes, targetWidth,
                           targetHeight, pool, outBuffer)) {
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
  if (!AcquireOutputBuffer(pool, requiredBytes, outBuffer)) {
    return false;
  }

  if (srcWidth == targetWidth && srcHeight == targetHeight) {
    for (int y = 0; y < targetHeight; ++y) {
      const auto* const srcRow =
          srcData + static_cast<std::size_t>(y) * static_cast<std::size_t>(srcStrideBytes);
      auto* const dstRow = outBuffer->data + static_cast<std::size_t>(y) * targetStrideBytes;
      CopyBytesSimd128(srcRow, dstRow, targetStrideBytes);
    }
    return true;
  }

  // CPU fallback 在发布到 frame_bridge 前就压成紧凑小帧，避免运行期继续持有大样本。
  // 最近邻步进器把比例除法移出像素内层，降低 compact CPU fallback 的热路径成本。
  const std::vector<int>& xIndices = BuildNearestSourceIndexTable(srcWidth, targetWidth);
  const std::vector<int>& yIndices = BuildNearestSourceIndexTable(srcHeight, targetHeight);
  const std::vector<std::size_t>& srcByteOffsets =
      BuildRgbaSourceByteOffsetTable(srcWidth, targetWidth, xIndices);
  if (xIndices.size() != static_cast<std::size_t>(targetWidth) ||
      yIndices.size() != static_cast<std::size_t>(targetHeight) ||
      srcByteOffsets.size() != static_cast<std::size_t>(targetWidth)) {
    return false;
  }
  for (int y = 0; y < targetHeight; ++y) {
    const int srcY = yIndices[static_cast<std::size_t>(y)];
    const auto* const srcRow =
        srcData + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(srcStrideBytes);
    auto* const dstRow = outBuffer->data + static_cast<std::size_t>(y) * targetStrideBytes;
    auto* dstPixel = dstRow;
    for (int x = 0; x < targetWidth; ++x) {
      const auto* const srcPixel = srcRow + srcByteOffsets[static_cast<std::size_t>(x)];
      std::memcpy(dstPixel, srcPixel, 4U);
      dstPixel += 4U;
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
                                  CpuFrameBufferPool* const pool,
                                  CompactCpuFrameBuffer* const outBuffer) noexcept {
  if (!IsValidScaleRequest(srcYPlaneData, srcWidth, srcHeight, srcYStrideBytes, targetWidth,
                           targetHeight, pool, outBuffer) ||
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
  if (!AcquireOutputBuffer(pool, yPlaneBytes + uvPlaneBytes, outBuffer)) {
    return false;
  }

  if (srcWidth == targetWidth && srcHeight == targetHeight) {
    for (int y = 0; y < targetHeight; ++y) {
      const auto* const srcRow =
          srcYPlaneData + static_cast<std::size_t>(y) * static_cast<std::size_t>(srcYStrideBytes);
      auto* const dstRow = outBuffer->data + static_cast<std::size_t>(y) * yStrideBytes;
      CopyBytesSimd128(srcRow, dstRow, yStrideBytes);
    }
    auto* const dstUvBase = outBuffer->data + yPlaneBytes;
    for (int y = 0; y < targetHeight / 2; ++y) {
      const auto* const srcRow = srcUvPlaneData +
                                 static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(srcUvStrideBytes);
      auto* const dstRow = dstUvBase + static_cast<std::size_t>(y) * uvStrideBytes;
      CopyBytesSimd128(srcRow, dstRow, uvStrideBytes);
    }
    return true;
  }

  const std::vector<int>& yIndices = BuildNearestSourceIndexTable(srcHeight, targetHeight);
  const std::vector<int>& xIndices = BuildNearestSourceIndexTable(srcWidth, targetWidth);
  if (xIndices.size() != static_cast<std::size_t>(targetWidth) ||
      yIndices.size() != static_cast<std::size_t>(targetHeight)) {
    return false;
  }
  for (int y = 0; y < targetHeight; ++y) {
    const int srcY = yIndices[static_cast<std::size_t>(y)];
    const auto* const srcRow =
        srcYPlaneData + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(srcYStrideBytes);
    auto* const dstRow = outBuffer->data + static_cast<std::size_t>(y) * yStrideBytes;
    for (int x = 0; x < targetWidth; ++x) {
      const int srcX = xIndices[static_cast<std::size_t>(x)];
      dstRow[x] = srcRow[srcX];
    }
  }

  const int srcUvWidthSamples = srcWidth / 2;
  const int srcUvHeightSamples = srcHeight / 2;
  const int targetUvWidthSamples = targetWidth / 2;
  const int targetUvHeightSamples = targetHeight / 2;
  auto* const dstUvBase = outBuffer->data + yPlaneBytes;
  const std::vector<int>& uvYIndices =
      BuildNearestSourceIndexTable(srcUvHeightSamples, targetUvHeightSamples);
  const std::vector<int>& uvXIndices =
      BuildNearestSourceIndexTable(srcUvWidthSamples, targetUvWidthSamples);
  const std::vector<std::size_t>& uvSrcByteOffsets = BuildInterleavedUvSourceByteOffsetTable(
      srcUvWidthSamples, targetUvWidthSamples, uvXIndices);
  if (uvXIndices.size() != static_cast<std::size_t>(targetUvWidthSamples) ||
      uvYIndices.size() != static_cast<std::size_t>(targetUvHeightSamples) ||
      uvSrcByteOffsets.size() != static_cast<std::size_t>(targetUvWidthSamples)) {
    return false;
  }
  for (int y = 0; y < targetUvHeightSamples; ++y) {
    const int srcY = uvYIndices[static_cast<std::size_t>(y)];
    const auto* const srcRow = srcUvPlaneData +
                               static_cast<std::size_t>(srcY) *
                                    static_cast<std::size_t>(srcUvStrideBytes);
    auto* const dstRow = dstUvBase + static_cast<std::size_t>(y) * uvStrideBytes;
    auto* dstPixel = dstRow;
    for (int x = 0; x < targetUvWidthSamples; ++x) {
      const auto* const srcPixel = srcRow + uvSrcByteOffsets[static_cast<std::size_t>(x)];
      dstPixel[0] = srcPixel[0];
      dstPixel[1] = srcPixel[1];
      dstPixel += 2U;
    }
  }

  return true;
}

}  // namespace wallpaper
