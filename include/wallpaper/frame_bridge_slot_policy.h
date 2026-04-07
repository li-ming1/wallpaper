#pragma once

#include <cstddef>

namespace wallpaper::frame_bridge {

[[nodiscard]] constexpr std::size_t AdvancePublishSlot(const std::size_t currentSlot,
                                                       const std::size_t slotCount) noexcept {
  if (slotCount == 0) {
    return 0;
  }
  return (currentSlot + 1U) % slotCount;
}

}  // namespace wallpaper::frame_bridge
