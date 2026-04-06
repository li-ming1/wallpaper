#include "wallpaper/frame_bridge.h"

#include <atomic>
#include <memory>
#include <memory_resource>

namespace wallpaper::frame_bridge {
namespace {

struct FramePayload final {
  LatestFrame frame;
};

std::atomic<std::shared_ptr<const FramePayload>> g_latestPayload;
std::atomic<std::uint64_t> g_latestSequence{0};
std::pmr::synchronized_pool_resource g_payloadPool{};

[[nodiscard]] std::shared_ptr<FramePayload> MakeFramePayloadBase(const int width, const int height,
                                                                 const int strideBytes,
                                                                 const std::int64_t timestamp100ns,
                                                                 const std::uint64_t sequence) {
  std::pmr::polymorphic_allocator<FramePayload> allocator{&g_payloadPool};
  auto payload = std::allocate_shared<FramePayload>(allocator);
  payload->frame.width = width;
  payload->frame.height = height;
  payload->frame.strideBytes = strideBytes;
  payload->frame.timestamp100ns = timestamp100ns;
  payload->frame.sequence = sequence;
  return payload;
}

void PublishPayload(std::shared_ptr<const FramePayload> payload, const std::uint64_t sequence) {
  g_latestPayload.store(std::move(payload), std::memory_order_release);
  g_latestSequence.store(sequence, std::memory_order_release);
}

}  // namespace

void PublishLatestFrame(const int width, const int height, const int strideBytes,
                        const std::int64_t timestamp100ns, const std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels) {
  if (rgbaPixels == nullptr || width <= 0 || height <= 0 || strideBytes <= 0) {
    return;
  }

  auto payload = MakeFramePayloadBase(width, height, strideBytes, timestamp100ns, sequence);
  payload->frame.gpuBacked = false;
  payload->frame.pixelFormat = PixelFormat::kRgba32;
  payload->frame.gpuSubresourceIndex = 0;
  payload->frame.dxgiFormat = 0;
  payload->frame.gpuTexture = nullptr;
  payload->frame.gpuTextureHolder.reset();
  payload->frame.yPlaneData = nullptr;
  payload->frame.yPlaneBytes = 0;
  payload->frame.yPlaneStrideBytes = 0;
  payload->frame.uvPlaneData = nullptr;
  payload->frame.uvPlaneBytes = 0;
  payload->frame.uvPlaneStrideBytes = 0;
  payload->frame.rgbaData = nullptr;
  payload->frame.rgbaDataBytes = 0;
  payload->frame.rgbaDataHolder.reset();
  if (rgbaPixels != nullptr && !rgbaPixels->empty()) {
    payload->frame.rgbaData = rgbaPixels->data();
    payload->frame.rgbaDataBytes = rgbaPixels->size();
    payload->frame.rgbaDataHolder =
        std::shared_ptr<void>(std::move(rgbaPixels),
                              const_cast<std::uint8_t*>(payload->frame.rgbaData));
  }
  PublishPayload(std::const_pointer_cast<const FramePayload>(std::move(payload)), sequence);
}

void PublishLatestFrameView(const int width, const int height, const int strideBytes,
                            const std::int64_t timestamp100ns, const std::uint64_t sequence,
                            const std::uint8_t* rgbaData, const std::size_t rgbaDataBytes,
                            std::shared_ptr<void> rgbaDataHolder) {
  if (width <= 0 || height <= 0 || strideBytes <= 0 || rgbaData == nullptr ||
      rgbaDataBytes == 0 || rgbaDataHolder == nullptr) {
    return;
  }

  auto payload = MakeFramePayloadBase(width, height, strideBytes, timestamp100ns, sequence);
  payload->frame.gpuBacked = false;
  payload->frame.pixelFormat = PixelFormat::kRgba32;
  payload->frame.gpuSubresourceIndex = 0;
  payload->frame.dxgiFormat = 0;
  payload->frame.gpuTexture = nullptr;
  payload->frame.gpuTextureHolder.reset();
  payload->frame.yPlaneData = nullptr;
  payload->frame.yPlaneBytes = 0;
  payload->frame.yPlaneStrideBytes = 0;
  payload->frame.uvPlaneData = nullptr;
  payload->frame.uvPlaneBytes = 0;
  payload->frame.uvPlaneStrideBytes = 0;
  payload->frame.rgbaData = rgbaData;
  payload->frame.rgbaDataBytes = rgbaDataBytes;
  payload->frame.rgbaDataHolder = std::move(rgbaDataHolder);
  PublishPayload(std::const_pointer_cast<const FramePayload>(std::move(payload)), sequence);
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

  auto payload =
      MakeFramePayloadBase(width, height, yPlaneStrideBytes, timestamp100ns, sequence);
  payload->frame.gpuBacked = false;
  payload->frame.pixelFormat = PixelFormat::kNv12;
  payload->frame.gpuSubresourceIndex = 0;
  payload->frame.dxgiFormat = 0;
  payload->frame.gpuTexture = nullptr;
  payload->frame.gpuAuxTexture = nullptr;
  payload->frame.gpuTextureHolder.reset();
  payload->frame.gpuAuxTextureHolder.reset();
  payload->frame.rgbaData = nullptr;
  payload->frame.rgbaDataBytes = 0;
  payload->frame.rgbaDataHolder = std::move(planeDataHolder);
  payload->frame.yPlaneData = yPlaneData;
  payload->frame.yPlaneBytes = yPlaneBytes;
  payload->frame.yPlaneStrideBytes = yPlaneStrideBytes;
  payload->frame.uvPlaneData = uvPlaneData;
  payload->frame.uvPlaneBytes = uvPlaneBytes;
  payload->frame.uvPlaneStrideBytes = uvPlaneStrideBytes;
  PublishPayload(std::const_pointer_cast<const FramePayload>(std::move(payload)), sequence);
}

void PublishLatestGpuFrame(const int width, const int height, const std::int64_t timestamp100ns,
                           const std::uint64_t sequence, const std::uint32_t dxgiFormat,
                           const std::uint32_t subresourceIndex,
                           std::shared_ptr<void> gpuTextureHolder) {
  if (width <= 0 || height <= 0 || gpuTextureHolder == nullptr) {
    return;
  }

  auto payload = MakeFramePayloadBase(width, height, 0, timestamp100ns, sequence);
  payload->frame.gpuBacked = true;
  payload->frame.pixelFormat = PixelFormat::kRgba32;
  payload->frame.gpuSubresourceIndex = subresourceIndex;
  payload->frame.dxgiFormat = dxgiFormat;
  payload->frame.gpuTexture = static_cast<ID3D11Texture2D*>(gpuTextureHolder.get());
  payload->frame.gpuAuxTexture = nullptr;
  payload->frame.gpuTextureHolder = std::move(gpuTextureHolder);
  payload->frame.gpuAuxTextureHolder.reset();
  payload->frame.rgbaData = nullptr;
  payload->frame.rgbaDataBytes = 0;
  payload->frame.rgbaDataHolder.reset();
  payload->frame.yPlaneData = nullptr;
  payload->frame.yPlaneBytes = 0;
  payload->frame.yPlaneStrideBytes = 0;
  payload->frame.uvPlaneData = nullptr;
  payload->frame.uvPlaneBytes = 0;
  payload->frame.uvPlaneStrideBytes = 0;
  PublishPayload(std::const_pointer_cast<const FramePayload>(std::move(payload)), sequence);
}

void PublishLatestGpuNv12Frame(const int width, const int height,
                               const std::int64_t timestamp100ns, const std::uint64_t sequence,
                               std::shared_ptr<void> yTextureHolder,
                               std::shared_ptr<void> uvTextureHolder) {
  if (width <= 0 || height <= 0 || yTextureHolder == nullptr || uvTextureHolder == nullptr) {
    return;
  }

  auto payload = MakeFramePayloadBase(width, height, width, timestamp100ns, sequence);
  payload->frame.gpuBacked = true;
  payload->frame.pixelFormat = PixelFormat::kNv12;
  payload->frame.gpuSubresourceIndex = 0;
  payload->frame.dxgiFormat = 0;
  payload->frame.gpuTexture = static_cast<ID3D11Texture2D*>(yTextureHolder.get());
  payload->frame.gpuAuxTexture = static_cast<ID3D11Texture2D*>(uvTextureHolder.get());
  payload->frame.gpuTextureHolder = std::move(yTextureHolder);
  payload->frame.gpuAuxTextureHolder = std::move(uvTextureHolder);
  payload->frame.rgbaData = nullptr;
  payload->frame.rgbaDataBytes = 0;
  payload->frame.rgbaDataHolder.reset();
  payload->frame.yPlaneData = nullptr;
  payload->frame.yPlaneBytes = 0;
  payload->frame.yPlaneStrideBytes = 0;
  payload->frame.uvPlaneData = nullptr;
  payload->frame.uvPlaneBytes = 0;
  payload->frame.uvPlaneStrideBytes = 0;
  PublishPayload(std::const_pointer_cast<const FramePayload>(std::move(payload)), sequence);
}

bool TryGetLatestFrame(LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }

  const auto payload = g_latestPayload.load(std::memory_order_acquire);
  if (payload == nullptr) {
    return false;
  }
  if (!payload->frame.gpuBacked && payload->frame.rgbaDataHolder == nullptr) {
    return false;
  }
  if (payload->frame.gpuBacked &&
      (payload->frame.gpuTextureHolder == nullptr ||
       (payload->frame.pixelFormat == PixelFormat::kNv12 &&
        payload->frame.gpuAuxTextureHolder == nullptr))) {
    return false;
  }
  *outFrame = payload->frame;
  return true;
}

bool TryGetLatestFrameIfNewer(const std::uint64_t lastSeenSequence, LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }
  if (g_latestSequence.load(std::memory_order_acquire) <= lastSeenSequence) {
    return false;
  }

  const auto payload = g_latestPayload.load(std::memory_order_acquire);
  if (payload == nullptr || payload->frame.sequence <= lastSeenSequence) {
    return false;
  }
  if (!payload->frame.gpuBacked && payload->frame.rgbaDataHolder == nullptr) {
    return false;
  }
  if (payload->frame.gpuBacked &&
      (payload->frame.gpuTextureHolder == nullptr ||
       (payload->frame.pixelFormat == PixelFormat::kNv12 &&
        payload->frame.gpuAuxTextureHolder == nullptr))) {
    return false;
  }
  *outFrame = payload->frame;
  return true;
}

void ReleaseLatestFrameIfSequenceConsumed(const std::uint64_t consumedSequence) {
  if (consumedSequence == 0) {
    return;
  }

  auto expected = g_latestPayload.load(std::memory_order_acquire);
  if (expected == nullptr || expected->frame.sequence != consumedSequence) {
    return;
  }

  const std::shared_ptr<const FramePayload> cleared{};
  if (!g_latestPayload.compare_exchange_strong(expected, cleared, std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
    return;
  }

  std::uint64_t expectedSequence = consumedSequence;
  (void)g_latestSequence.compare_exchange_strong(expectedSequence, 0, std::memory_order_release,
                                                 std::memory_order_relaxed);
}

void ClearLatestFrame() {
  g_latestPayload.store(std::shared_ptr<const FramePayload>{}, std::memory_order_release);
  g_latestSequence.store(0, std::memory_order_release);
}

}  // namespace wallpaper::frame_bridge
