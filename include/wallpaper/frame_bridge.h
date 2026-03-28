#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#ifdef _WIN32
struct ID3D11Texture2D;
#endif

namespace wallpaper::frame_bridge {

struct LatestFrame final {
  int width = 0;
  int height = 0;
  int strideBytes = 0;
  std::int64_t timestamp100ns = 0;
  std::uint64_t sequence = 0;
  bool gpuBacked = false;
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
  std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels;
};

void PublishLatestFrame(int width, int height, int strideBytes, std::int64_t timestamp100ns,
                        std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels);
void PublishLatestFrameView(int width, int height, int strideBytes, std::int64_t timestamp100ns,
                            std::uint64_t sequence, const std::uint8_t* rgbaData,
                            std::size_t rgbaDataBytes, std::shared_ptr<void> rgbaDataHolder);
void PublishLatestGpuFrame(int width, int height, std::int64_t timestamp100ns,
                           std::uint64_t sequence, std::uint32_t dxgiFormat,
                           std::uint32_t subresourceIndex,
                           std::shared_ptr<void> gpuTextureHolder);
[[nodiscard]] bool TryGetLatestFrame(LatestFrame* outFrame);
void ClearLatestFrame();

}  // namespace wallpaper::frame_bridge
