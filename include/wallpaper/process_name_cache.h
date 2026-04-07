#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace wallpaper {

enum class ProcessNameCacheLookup {
  kMiss = 0,
  kHit = 1,
  kKnownFailure = 2,
};

class ProcessNameCache final {
 public:
  static constexpr std::size_t kCapacity = 4;

  [[nodiscard]] std::size_t Capacity() const noexcept { return kCapacity; }

  [[nodiscard]] ProcessNameCacheLookup TryGet(const std::uint32_t processId,
                                              std::wstring* const outName) const {
    for (const Entry& entry : entries_) {
      if (!entry.valid || entry.processId != processId) {
        continue;
      }
      if (!entry.lookupOk) {
        if (outName != nullptr) {
          outName->clear();
        }
        return ProcessNameCacheLookup::kKnownFailure;
      }
      if (outName != nullptr) {
        *outName = entry.processName;
      }
      return ProcessNameCacheLookup::kHit;
    }
    return ProcessNameCacheLookup::kMiss;
  }

  void RememberSuccess(const std::uint32_t processId, std::wstring processName) {
    Store(processId, true, std::move(processName));
  }

  void RememberFailure(const std::uint32_t processId) { Store(processId, false, {}); }

 private:
  struct Entry final {
    std::uint32_t processId = 0;
    bool valid = false;
    bool lookupOk = false;
    std::wstring processName;
  };

  void Store(const std::uint32_t processId, const bool lookupOk, std::wstring processName) {
    for (Entry& entry : entries_) {
      if (entry.valid && entry.processId == processId) {
        entry.lookupOk = lookupOk;
        entry.processName = std::move(processName);
        return;
      }
    }

    Entry& entry = entries_[nextReplaceIndex_];
    entry.processId = processId;
    entry.valid = true;
    entry.lookupOk = lookupOk;
    entry.processName = std::move(processName);
    nextReplaceIndex_ = (nextReplaceIndex_ + 1U) % entries_.size();
  }

  std::array<Entry, kCapacity> entries_{};
  std::size_t nextReplaceIndex_ = 0;
};

}  // namespace wallpaper
