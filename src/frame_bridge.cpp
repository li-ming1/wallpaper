#include "wallpaper/frame_bridge.h"

#include "wallpaper/frame_bridge_slot_policy.h"

#include <array>
#include <atomic>
#include <mutex>

namespace wallpaper::frame_bridge {
namespace {

constexpr std::size_t kFrameSlotCount = 3;

std::array<LatestFrame, kFrameSlotCount> g_frameSlots{};
std::atomic<std::uint64_t> g_latestSequence{0};
std::mutex g_frameSlotsMu;
std::size_t g_latestSlot = 0;
bool g_hasLatestFrame = false;

[[nodiscard]] LatestFrame MakeFrameBase(const int width, const int height, const int strideBytes,
                                        const std::int64_t timestamp100ns,
                                        const std::uint64_t sequence) noexcept {
  LatestFrame frame;
  frame.width = width;
  frame.height = height;
  frame.strideBytes = strideBytes;
  frame.timestamp100ns = timestamp100ns;
  frame.sequence = sequence;
  return frame;
}

[[nodiscard]] bool IsReadableFrame(const LatestFrame& frame) noexcept {
  if (frame.sequence == 0) {
    return false;
  }
  if (!frame.gpuBacked) {
    return frame.rgbaDataHolder != nullptr;
  }
  if (frame.gpuTextureHolder == nullptr) {
    return false;
  }
  if (frame.pixelFormat == PixelFormat::kNv12 && frame.gpuAuxTextureHolder == nullptr) {
    return false;
  }
  return true;
}

void PublishFrame(LatestFrame frame) {
  std::lock_guard<std::mutex> lock(g_frameSlotsMu);
  const std::size_t currentSlot = g_hasLatestFrame ? g_latestSlot : (kFrameSlotCount - 1U);
  const std::size_t nextSlot = AdvancePublishSlot(currentSlot, kFrameSlotCount);
  g_frameSlots[nextSlot] = std::move(frame);
  g_latestSlot = nextSlot;
  g_hasLatestFrame = true;
  g_latestSequence.store(g_frameSlots[nextSlot].sequence, std::memory_order_release);
}

}  // namespace

void PublishLatestFrame(const int width, const int height, const int strideBytes,
                        const std::int64_t timestamp100ns, const std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels) {
  if (rgbaPixels == nullptr || width <= 0 || height <= 0 || strideBytes <= 0) {
    return;
  }

  LatestFrame frame = MakeFrameBase(width, height, strideBytes, timestamp100ns, sequence);
  frame.gpuBacked = false;
  frame.pixelFormat = PixelFormat::kRgba32;
  if (!rgbaPixels->empty()) {
    frame.rgbaData = rgbaPixels->data();
    frame.rgbaDataBytes = rgbaPixels->size();
    frame.rgbaDataHolder =
        std::shared_ptr<void>(std::move(rgbaPixels), const_cast<std::uint8_t*>(frame.rgbaData));
  }
  PublishFrame(std::move(frame));
}

void PublishLatestFrameView(const int width, const int height, const int strideBytes,
                            const std::int64_t timestamp100ns, const std::uint64_t sequence,
                            const std::uint8_t* rgbaData, const std::size_t rgbaDataBytes,
                            std::shared_ptr<void> rgbaDataHolder) {
  if (width <= 0 || height <= 0 || strideBytes <= 0 || rgbaData == nullptr ||
      rgbaDataBytes == 0 || rgbaDataHolder == nullptr) {
    return;
  }

  LatestFrame frame = MakeFrameBase(width, height, strideBytes, timestamp100ns, sequence);
  frame.gpuBacked = false;
  frame.pixelFormat = PixelFormat::kRgba32;
  frame.rgbaData = rgbaData;
  frame.rgbaDataBytes = rgbaDataBytes;
  frame.rgbaDataHolder = std::move(rgbaDataHolder);
  PublishFrame(std::move(frame));
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

  LatestFrame frame = MakeFrameBase(width, height, yPlaneStrideBytes, timestamp100ns, sequence);
  frame.gpuBacked = false;
  frame.pixelFormat = PixelFormat::kNv12;
  frame.rgbaDataHolder = std::move(planeDataHolder);
  frame.yPlaneData = yPlaneData;
  frame.yPlaneBytes = yPlaneBytes;
  frame.yPlaneStrideBytes = yPlaneStrideBytes;
  frame.uvPlaneData = uvPlaneData;
  frame.uvPlaneBytes = uvPlaneBytes;
  frame.uvPlaneStrideBytes = uvPlaneStrideBytes;
  PublishFrame(std::move(frame));
}

void PublishLatestGpuFrame(const int width, const int height, const std::int64_t timestamp100ns,
                           const std::uint64_t sequence, const std::uint32_t dxgiFormat,
                           const std::uint32_t subresourceIndex,
                           std::shared_ptr<void> gpuTextureHolder) {
  if (width <= 0 || height <= 0 || gpuTextureHolder == nullptr) {
    return;
  }

  LatestFrame frame = MakeFrameBase(width, height, 0, timestamp100ns, sequence);
  frame.gpuBacked = true;
  frame.pixelFormat = PixelFormat::kRgba32;
  frame.gpuSubresourceIndex = subresourceIndex;
  frame.dxgiFormat = dxgiFormat;
  frame.gpuTexture = static_cast<ID3D11Texture2D*>(gpuTextureHolder.get());
  frame.gpuTextureHolder = std::move(gpuTextureHolder);
  PublishFrame(std::move(frame));
}

void PublishLatestGpuNv12Frame(const int width, const int height,
                               const std::int64_t timestamp100ns, const std::uint64_t sequence,
                               std::shared_ptr<void> yTextureHolder,
                               std::shared_ptr<void> uvTextureHolder) {
  if (width <= 0 || height <= 0 || yTextureHolder == nullptr || uvTextureHolder == nullptr) {
    return;
  }

  LatestFrame frame = MakeFrameBase(width, height, width, timestamp100ns, sequence);
  frame.gpuBacked = true;
  frame.pixelFormat = PixelFormat::kNv12;
  frame.gpuTexture = static_cast<ID3D11Texture2D*>(yTextureHolder.get());
  frame.gpuAuxTexture = static_cast<ID3D11Texture2D*>(uvTextureHolder.get());
  frame.gpuTextureHolder = std::move(yTextureHolder);
  frame.gpuAuxTextureHolder = std::move(uvTextureHolder);
  PublishFrame(std::move(frame));
}

bool TryGetLatestFrame(LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_frameSlotsMu);
  if (!g_hasLatestFrame) {
    return false;
  }
  const LatestFrame& latest = g_frameSlots[g_latestSlot];
  if (!IsReadableFrame(latest)) {
    return false;
  }
  *outFrame = latest;
  return true;
}

bool TryGetLatestFrameIfNewer(const std::uint64_t lastSeenSequence, LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }
  if (g_latestSequence.load(std::memory_order_acquire) <= lastSeenSequence) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_frameSlotsMu);
  if (!g_hasLatestFrame) {
    return false;
  }
  const LatestFrame& latest = g_frameSlots[g_latestSlot];
  if (latest.sequence <= lastSeenSequence || !IsReadableFrame(latest)) {
    return false;
  }
  *outFrame = latest;
  return true;
}

void ReleaseLatestFrameIfSequenceConsumed(const std::uint64_t consumedSequence) {
  if (consumedSequence == 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_frameSlotsMu);
  if (!g_hasLatestFrame) {
    return;
  }
  if (g_frameSlots[g_latestSlot].sequence != consumedSequence) {
    return;
  }
  g_frameSlots[g_latestSlot] = LatestFrame{};
  g_hasLatestFrame = false;
  g_latestSequence.store(0, std::memory_order_release);
}

void ClearLatestFrame() {
  std::lock_guard<std::mutex> lock(g_frameSlotsMu);
  for (LatestFrame& slot : g_frameSlots) {
    slot = LatestFrame{};
  }
  g_hasLatestFrame = false;
  g_latestSlot = 0;
  g_latestSequence.store(0, std::memory_order_release);
}

}  // namespace wallpaper::frame_bridge
