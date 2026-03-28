#include "wallpaper/frame_bridge.h"

#include <mutex>

namespace wallpaper::frame_bridge {
namespace {

std::mutex g_frameMutex;
LatestFrame g_latestFrame;
bool g_hasFrame = false;

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
  g_latestFrame.gpuSubresourceIndex = 0;
  g_latestFrame.dxgiFormat = 0;
  g_latestFrame.gpuTexture = nullptr;
  g_latestFrame.gpuTextureHolder.reset();
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
  g_latestFrame.gpuSubresourceIndex = 0;
  g_latestFrame.dxgiFormat = 0;
  g_latestFrame.gpuTexture = nullptr;
  g_latestFrame.gpuTextureHolder.reset();
  g_latestFrame.rgbaPixels.reset();
  g_latestFrame.rgbaData = rgbaData;
  g_latestFrame.rgbaDataBytes = rgbaDataBytes;
  g_latestFrame.rgbaDataHolder = std::move(rgbaDataHolder);
  g_hasFrame = true;
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
  g_hasFrame = true;
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

void ClearLatestFrame() {
  std::lock_guard<std::mutex> lock(g_frameMutex);
  g_latestFrame = LatestFrame{};
  g_hasFrame = false;
}

}  // namespace wallpaper::frame_bridge
