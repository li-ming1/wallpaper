#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace wallpaper::frame_bridge {

struct LatestFrame final {
  int width = 0;
  int height = 0;
  int strideBytes = 0;
  std::int64_t timestamp100ns = 0;
  std::uint64_t sequence = 0;
  std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels;
};

void PublishLatestFrame(int width, int height, int strideBytes, std::int64_t timestamp100ns,
                        std::uint64_t sequence,
                        std::shared_ptr<const std::vector<std::uint8_t>> rgbaPixels);
[[nodiscard]] bool TryGetLatestFrame(LatestFrame* outFrame);
void ClearLatestFrame();

}  // namespace wallpaper::frame_bridge
