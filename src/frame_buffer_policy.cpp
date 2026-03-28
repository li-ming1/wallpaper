#include "wallpaper/frame_buffer_policy.h"

namespace wallpaper {

std::size_t DecideFrameBufferCapacity(const std::size_t currentCapacity,
                                      const std::size_t requiredSize) noexcept {
  if (requiredSize == 0U) {
    return 0U;
  }
  if (currentCapacity < requiredSize) {
    return requiredSize;
  }
  // 仅在容量超过需求 2 倍时缩容，避免在相邻分辨率间频繁抖动。
  if (currentCapacity > requiredSize * 2U) {
    return requiredSize;
  }
  return currentCapacity;
}

}  // namespace wallpaper
