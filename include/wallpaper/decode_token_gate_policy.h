#pragma once

#include <cstdint>

namespace wallpaper {

// 在无新解码序列时跳过互斥区，降低渲染热路径的无效加锁成本。
[[nodiscard]] inline bool ShouldAttemptDecodedTokenConsume(
    const bool hasLastPresentedFrame, const std::uint64_t lastPresentedSequence,
    const std::uint64_t latestDecodedSequence) noexcept {
  if (latestDecodedSequence == 0) {
    return false;
  }
  if (!hasLastPresentedFrame) {
    return true;
  }
  return latestDecodedSequence != lastPresentedSequence;
}

}  // namespace wallpaper

