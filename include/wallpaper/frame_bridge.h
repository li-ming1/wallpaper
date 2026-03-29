#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#ifdef _WIN32
struct ID3D11Texture2D;
#endif

namespace wallpaper::frame_bridge {

enum class PixelFormat {
  kUnknown = 0,
  kRgba32 = 1,
  kNv12 = 2,
};

struct LatestFrame final {
  int width = 0;
  int height = 0;
  int strideBytes = 0;
  std::int64_t timestamp100ns = 0;
  std::uint64_t sequence = 0;
  bool gpuBacked = false;
  PixelFormat pixelFormat = PixelFormat::kUnknown;
  std::uint32_t gpuSubresourceIndex = 0;
  std::uint32_t dxgiFormat = 0;
#ifdef _WIN32
  ID3D11Texture2D* gpuTexture = nullptr;
#else
  void* gpuTexture = nullptr;
#endif
  std::shared_ptr<void> gpuTextureHolder;
  const std::uint8_t* rgbaData = nullptr;
  std::size_t rgbaDataBytes = 0;
  std::shared_ptr<void> rgbaDataHolder;
  const std::uint8_t* yPlaneData = nullptr;
  std::size_t yPlaneBytes = 0;
  int yPlaneStrideBytes = 0;
  const std::uint8_t* uvPlaneData = nullptr;
  std::size_t uvPlaneBytes = 0;
  int uvPlaneStrideBytes = 0;
};

void PublishLatestFrame(int width, int height, int strideBytes, std::int64_t timestamp100ns,
                        std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels);
void PublishLatestFrameView(int width, int height, int strideBytes, std::int64_t timestamp100ns,
                            std::uint64_t sequence, const std::uint8_t* rgbaData,
                            std::size_t rgbaDataBytes, std::shared_ptr<void> rgbaDataHolder);
void PublishLatestNv12FrameView(int width, int height, int yPlaneStrideBytes,
                                int uvPlaneStrideBytes, std::int64_t timestamp100ns,
                                std::uint64_t sequence, const std::uint8_t* yPlaneData,
                                std::size_t yPlaneBytes, const std::uint8_t* uvPlaneData,
                                std::size_t uvPlaneBytes, std::shared_ptr<void> planeDataHolder);
void PublishLatestGpuFrame(int width, int height, std::int64_t timestamp100ns,
                           std::uint64_t sequence, std::uint32_t dxgiFormat,
                           std::uint32_t subresourceIndex,
                           std::shared_ptr<void> gpuTextureHolder);
[[nodiscard]] bool TryGetLatestFrame(LatestFrame* outFrame);
[[nodiscard]] bool TryGetLatestFrameIfNewer(std::uint64_t lastSeenSequence,
                                            LatestFrame* outFrame);
void ReleaseLatestFrameIfSequenceConsumed(std::uint64_t consumedSequence);
void ClearLatestFrame();

}  // namespace wallpaper::frame_bridge
