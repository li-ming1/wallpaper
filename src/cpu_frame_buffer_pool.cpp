#include "wallpaper/cpu_frame_buffer_pool.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace wallpaper {
namespace {

struct CpuFrameBufferBlock final {
  std::vector<std::uint8_t> storage;
  bool inUse = false;
};

}  // namespace

struct CpuFrameBufferPool::State final
    : std::enable_shared_from_this<CpuFrameBufferPool::State> {
  explicit State(const std::size_t maxBlocks)
      : maxRetainedBlocks(std::max<std::size_t>(1, maxBlocks)) {}

  [[nodiscard]] CpuFrameBufferLease Acquire(const std::size_t minBytes) {
    if (minBytes == 0) {
      return {};
    }

    {
      std::lock_guard<std::mutex> lock(mu);
      CpuFrameBufferBlock* selected = FindFreeBlock(minBytes);
      if (selected == nullptr && blocks.size() < maxRetainedBlocks) {
        blocks.push_back(std::make_unique<CpuFrameBufferBlock>());
        selected = blocks.back().get();
      }
      if (selected != nullptr) {
        selected->inUse = true;
        if (selected->storage.size() < minBytes) {
          selected->storage.resize(minBytes);
        }
        return CpuFrameBufferLease{
            selected->storage.data(),
            selected->storage.size(),
            // holder 释放时仅把块归还到池中，底层内存继续保留给后续同尺寸帧复用。
            std::shared_ptr<void>(selected->storage.data(),
                                  [state = shared_from_this(), selected](void*) {
                                    std::lock_guard<std::mutex> releaseLock(state->mu);
                                    selected->inUse = false;
                                  }),
        };
      }
    }

    auto transientStorage = std::make_shared<std::vector<std::uint8_t>>(minBytes);
    return CpuFrameBufferLease{
        transientStorage->data(),
        transientStorage->size(),
        std::shared_ptr<void>(transientStorage, transientStorage->data()),
    };
  }

  [[nodiscard]] CpuFrameBufferBlock* FindFreeBlock(const std::size_t minBytes) noexcept {
    CpuFrameBufferBlock* fallback = nullptr;
    for (const auto& block : blocks) {
      if (block->inUse) {
        continue;
      }
      if (block->storage.size() >= minBytes) {
        return block.get();
      }
      if (fallback == nullptr) {
        fallback = block.get();
      }
    }
    return fallback;
  }

  std::mutex mu;
  std::size_t maxRetainedBlocks = 1;
  std::vector<std::unique_ptr<CpuFrameBufferBlock>> blocks;
};

CpuFrameBufferPool::CpuFrameBufferPool(const std::size_t maxRetainedBlocks)
    : state_(std::make_shared<State>(maxRetainedBlocks)) {}

CpuFrameBufferLease CpuFrameBufferPool::Acquire(const std::size_t minBytes) {
  return state_ != nullptr ? state_->Acquire(minBytes) : CpuFrameBufferLease{};
}

}  // namespace wallpaper
