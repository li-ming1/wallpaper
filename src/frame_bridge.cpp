#include "wallpaper/frame_bridge.h"

#include <atomic>
#include <mutex>

namespace wallpaper::frame_bridge {
namespace {

std::mutex g_frameMutex;
LatestFrame g_latestFrame;
bool g_hasFrame = false;
std::atomic<std::uint64_t> g_latestSequence{0};

}  // namespace

void PublishLatestFrame(const int width, const int height, const int strideBytes,
                        const std::int64_t timestamp100ns, const std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels) {
  if (rgbaPixels == nullptr || width <= 0 || height <= 0 || strideBytes <= 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame.width = width;
  g_latestFrame.height = height;
  g_latestFrame.strideBytes = strideBytes;
  g_latestFrame.timestamp100ns = timestamp100ns;
  g_latestFrame.sequence = sequence;
  g_latestFrame.gpuBacked = false;
  g_latestFrame.pixelFormat = PixelFormat::kRgba32;
  g_latestFrame.gpuSubresourceIndex = 0;
  g_latestFrame.dxgiFormat = 0;
  g_latestFrame.gpuTexture = nullptr;
  g_latestFrame.gpuTextureHolder.reset();
  g_latestFrame.yPlaneData = nullptr;
  g_latestFrame.yPlaneBytes = 0;
  g_latestFrame.yPlaneStrideBytes = 0;
  g_latestFrame.uvPlaneData = nullptr;
  g_latestFrame.uvPlaneBytes = 0;
  g_latestFrame.uvPlaneStrideBytes = 0;
  g_latestFrame.rgbaData = nullptr;
  g_latestFrame.rgbaDataBytes = 0;
  g_latestFrame.rgbaDataHolder.reset();
  g_latestFrame.rgbaPixels = std::move(rgbaPixels);
  if (g_latestFrame.rgbaPixels != nullptr && !g_latestFrame.rgbaPixels->empty()) {
    g_latestFrame.rgbaData = g_latestFrame.rgbaPixels->data();
    g_latestFrame.rgbaDataBytes = g_latestFrame.rgbaPixels->size();
    g_latestFrame.rgbaDataHolder =
        std::shared_ptr<void>(g_latestFrame.rgbaPixels,
                              const_cast<std::uint8_t*>(g_latestFrame.rgbaPixels->data()));
  }
  g_hasFrame = true;
  g_latestSequence.store(sequence, std::memory_order_release);
}

void PublishLatestFrameView(const int width, const int height, const int strideBytes,
                            const std::int64_t timestamp100ns, const std::uint64_t sequence,
                            const std::uint8_t* rgbaData, const std::size_t rgbaDataBytes,
                            std::shared_ptr<void> rgbaDataHolder) {
  if (width <= 0 || height <= 0 || strideBytes <= 0 || rgbaData == nullptr ||
      rgbaDataBytes == 0 || rgbaDataHolder == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame.width = width;
  g_latestFrame.height = height;
  g_latestFrame.strideBytes = strideBytes;
  g_latestFrame.timestamp100ns = timestamp100ns;
  g_latestFrame.sequence = sequence;
  g_latestFrame.gpuBacked = false;
  g_latestFrame.pixelFormat = PixelFormat::kRgba32;
  g_latestFrame.gpuSubresourceIndex = 0;
  g_latestFrame.dxgiFormat = 0;
  g_latestFrame.gpuTexture = nullptr;
  g_latestFrame.gpuTextureHolder.reset();
  g_latestFrame.yPlaneData = nullptr;
  g_latestFrame.yPlaneBytes = 0;
  g_latestFrame.yPlaneStrideBytes = 0;
  g_latestFrame.uvPlaneData = nullptr;
  g_latestFrame.uvPlaneBytes = 0;
  g_latestFrame.uvPlaneStrideBytes = 0;
  g_latestFrame.rgbaPixels.reset();
  g_latestFrame.rgbaData = rgbaData;
  g_latestFrame.rgbaDataBytes = rgbaDataBytes;
  g_latestFrame.rgbaDataHolder = std::move(rgbaDataHolder);
  g_hasFrame = true;
  g_latestSequence.store(sequence, std::memory_order_release);
}

void PublishLatestNv12FrameView(const int width, const int height, const int yPlaneStrideBytes,
                                const int uvPlaneStrideBytes,
                                const std::int64_t timestamp100ns,
                                const std::uint64_t sequence,
                                const std::uint8_t* yPlaneData,
                                const std::size_t yPlaneBytes,
                                const std::uint8_t* uvPlaneData,
                                const std::size_t uvPlaneBytes,
                                std::shared_ptr<void> planeDataHolder) {
  if (width <= 0 || height <= 0 || yPlaneStrideBytes <= 0 || uvPlaneStrideBytes <= 0 ||
      yPlaneData == nullptr || uvPlaneData == nullptr || yPlaneBytes == 0 || uvPlaneBytes == 0 ||
      planeDataHolder == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame.width = width;
  g_latestFrame.height = height;
  g_latestFrame.strideBytes = yPlaneStrideBytes;
  g_latestFrame.timestamp100ns = timestamp100ns;
  g_latestFrame.sequence = sequence;
  g_latestFrame.gpuBacked = false;
  g_latestFrame.pixelFormat = PixelFormat::kNv12;
  g_latestFrame.gpuSubresourceIndex = 0;
  g_latestFrame.dxgiFormat = 0;
  g_latestFrame.gpuTexture = nullptr;
  g_latestFrame.gpuTextureHolder.reset();
  g_latestFrame.rgbaPixels.reset();
  g_latestFrame.rgbaData = nullptr;
  g_latestFrame.rgbaDataBytes = 0;
  g_latestFrame.rgbaDataHolder = std::move(planeDataHolder);
  g_latestFrame.yPlaneData = yPlaneData;
  g_latestFrame.yPlaneBytes = yPlaneBytes;
  g_latestFrame.yPlaneStrideBytes = yPlaneStrideBytes;
  g_latestFrame.uvPlaneData = uvPlaneData;
  g_latestFrame.uvPlaneBytes = uvPlaneBytes;
  g_latestFrame.uvPlaneStrideBytes = uvPlaneStrideBytes;
  g_hasFrame = true;
  g_latestSequence.store(sequence, std::memory_order_release);
}

void PublishLatestGpuFrame(const int width, const int height, const std::int64_t timestamp100ns,
                           const std::uint64_t sequence, const std::uint32_t dxgiFormat,
                           const std::uint32_t subresourceIndex,
                           std::shared_ptr<void> gpuTextureHolder) {
  if (width <= 0 || height <= 0 || gpuTextureHolder == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame.width = width;
  g_latestFrame.height = height;
  g_latestFrame.strideBytes = 0;
  g_latestFrame.timestamp100ns = timestamp100ns;
  g_latestFrame.sequence = sequence;
  g_latestFrame.gpuBacked = true;
  g_latestFrame.pixelFormat = PixelFormat::kRgba32;
  g_latestFrame.gpuSubresourceIndex = subresourceIndex;
  g_latestFrame.dxgiFormat = dxgiFormat;
#ifdef _WIN32
  g_latestFrame.gpuTexture = static_cast<ID3D11Texture2D*>(gpuTextureHolder.get());
#else
  g_latestFrame.gpuTexture = gpuTextureHolder.get();
#endif
  g_latestFrame.gpuTextureHolder = std::move(gpuTextureHolder);
  g_latestFrame.rgbaData = nullptr;
  g_latestFrame.rgbaDataBytes = 0;
  g_latestFrame.rgbaDataHolder.reset();
  g_latestFrame.rgbaPixels.reset();
  g_latestFrame.yPlaneData = nullptr;
  g_latestFrame.yPlaneBytes = 0;
  g_latestFrame.yPlaneStrideBytes = 0;
  g_latestFrame.uvPlaneData = nullptr;
  g_latestFrame.uvPlaneBytes = 0;
  g_latestFrame.uvPlaneStrideBytes = 0;
  g_hasFrame = true;
  g_latestSequence.store(sequence, std::memory_order_release);
}

bool TryGetLatestFrame(LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  if (!g_hasFrame) {
    return false;
  }
  if (!g_latestFrame.gpuBacked && g_latestFrame.rgbaDataHolder == nullptr) {
    return false;
  }
  if (g_latestFrame.gpuBacked && g_latestFrame.gpuTextureHolder == nullptr) {
    return false;
  }
  *outFrame = g_latestFrame;
  return true;
}

bool TryGetLatestFrameIfNewer(const std::uint64_t lastSeenSequence, LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }
  if (g_latestSequence.load(std::memory_order_acquire) <= lastSeenSequence) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  if (!g_hasFrame || g_latestFrame.sequence <= lastSeenSequence) {
    return false;
  }
  if (!g_latestFrame.gpuBacked && g_latestFrame.rgbaDataHolder == nullptr) {
    return false;
  }
  if (g_latestFrame.gpuBacked && g_latestFrame.gpuTextureHolder == nullptr) {
    return false;
  }
  *outFrame = g_latestFrame;
  return true;
}

void ReleaseLatestFrameIfSequenceConsumed(const std::uint64_t consumedSequence) {
  if (consumedSequence == 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  if (!g_hasFrame || g_latestFrame.sequence != consumedSequence) {
    return;
  }
  g_latestFrame = LatestFrame{};
  g_hasFrame = false;
  g_latestSequence.store(0, std::memory_order_release);
}

void ClearLatestFrame() {
  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame = LatestFrame{};
  g_hasFrame = false;
  g_latestSequence.store(0, std::memory_order_release);
}

}  // namespace wallpaper::frame_bridge
