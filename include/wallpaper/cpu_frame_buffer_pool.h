#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace wallpaper {

struct CpuFrameBufferLease final {
  std::uint8_t* data = nullptr;
  std::size_t capacityBytes = 0;
  std::shared_ptr<void> holder;
};

class CpuFrameBufferPool final {
 public:
  explicit CpuFrameBufferPool(std::size_t maxRetainedBlocks = 4);

  [[nodiscard]] CpuFrameBufferLease Acquire(std::size_t minBytes);

 private:
  struct State;

  std::shared_ptr<State> state_;
};

}  // namespace wallpaper
