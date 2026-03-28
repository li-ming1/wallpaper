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
  g_latestFrame.rgbaPixels = std::move(rgbaPixels);
  g_hasFrame = true;
}

bool TryGetLatestFrame(LatestFrame* outFrame) {
  if (outFrame == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_frameMutex);
  if (!g_hasFrame || g_latestFrame.rgbaPixels == nullptr) {
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
