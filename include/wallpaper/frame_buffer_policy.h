#pragma once

#include <cstddef>

namespace wallpaper {

// 复用策略：容量不足则扩容，容量远大于需求时回收，其他情况保持复用以减少分配抖动。
[[nodiscard]] std::size_t DecideFrameBufferCapacity(std::size_t currentCapacity,
                                                    std::size_t requiredSize) noexcept;

}  // namespace wallpaper
