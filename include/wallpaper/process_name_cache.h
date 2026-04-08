#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
  static constexpr std::size_t kHashBucketCount = kCapacity * 2;

  [[nodiscard]] std::size_t Capacity() const noexcept { return kCapacity; }

  [[nodiscard]] ProcessNameCacheLookup TryGet(const std::uint32_t processId,
                                              std::wstring* const outName) const {
    const std::size_t bucketIndex = FindBucketIndex(processId);
    if (bucketIndex == kInvalidIndex) {
      return ProcessNameCacheLookup::kMiss;
    }
    const Bucket& bucket = buckets_[bucketIndex];
    if (bucket.entryIndex >= entries_.size()) {
      return ProcessNameCacheLookup::kMiss;
    }
    const Entry& entry = entries_[bucket.entryIndex];
    if (!entry.valid || entry.processId != processId) {
      return ProcessNameCacheLookup::kMiss;
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

  void RememberSuccess(const std::uint32_t processId, std::wstring processName) {
    Store(processId, true, std::move(processName));
  }

  void RememberFailure(const std::uint32_t processId) { Store(processId, false, {}); }

 private:
  static constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

  struct Entry final {
    std::uint32_t processId = 0;
    bool valid = false;
    bool lookupOk = false;
    std::wstring processName;
    std::uint64_t generation = 0;
  };

  struct Bucket final {
    bool occupied = false;
    std::uint32_t processId = 0;
    std::size_t entryIndex = 0;
  };

  void Store(const std::uint32_t processId, const bool lookupOk, std::wstring processName) {
    const std::size_t existingBucket = FindBucketIndex(processId);
    if (existingBucket != kInvalidIndex) {
      const std::size_t existingEntry = buckets_[existingBucket].entryIndex;
      if (existingEntry < entries_.size()) {
        Entry& entry = entries_[existingEntry];
        entry.lookupOk = lookupOk;
        entry.processName = std::move(processName);
        entry.generation = nextGeneration_++;
        RebuildBuckets();
        return;
      }
    }

    std::size_t targetEntryIndex = kInvalidIndex;
    if (entryCount_ < entries_.size()) {
      for (std::size_t slot = 0; slot < entries_.size(); ++slot) {
        if (!entries_[slot].valid) {
          targetEntryIndex = slot;
          break;
        }
      }
      if (targetEntryIndex != kInvalidIndex) {
        ++entryCount_;
      }
    } else {
      std::uint64_t oldestGeneration = std::numeric_limits<std::uint64_t>::max();
      for (std::size_t slot = 0; slot < entries_.size(); ++slot) {
        const Entry& candidate = entries_[slot];
        if (!candidate.valid) {
          continue;
        }
        if (candidate.generation < oldestGeneration) {
          oldestGeneration = candidate.generation;
          targetEntryIndex = slot;
        }
      }
    }

    if (targetEntryIndex == kInvalidIndex) {
      return;
    }

    Entry& entry = entries_[targetEntryIndex];
    entry.processId = processId;
    entry.valid = true;
    entry.lookupOk = lookupOk;
    entry.processName = std::move(processName);
    entry.generation = nextGeneration_++;
    RebuildBuckets();
  }

  [[nodiscard]] static std::size_t HashProcessId(const std::uint32_t processId) noexcept {
    return static_cast<std::size_t>(processId * 2654435761U);
  }

  [[nodiscard]] std::size_t FindBucketIndex(const std::uint32_t processId) const noexcept {
    if (kHashBucketCount == 0) {
      return kInvalidIndex;
    }

    const std::size_t start = HashProcessId(processId) % kHashBucketCount;
    for (std::size_t probe = 0; probe < kHashBucketCount; ++probe) {
      const std::size_t index = (start + probe) % kHashBucketCount;
      const Bucket& bucket = buckets_[index];
      if (!bucket.occupied) {
        return kInvalidIndex;
      }
      if (bucket.processId == processId) {
        return index;
      }
    }
    return kInvalidIndex;
  }

  void RebuildBuckets() noexcept {
    for (std::size_t bucketIndex = 0; bucketIndex < buckets_.size(); ++bucketIndex) {
      buckets_[bucketIndex] = Bucket{};
    }
    for (std::size_t entryIndex = 0; entryIndex < entries_.size(); ++entryIndex) {
      const Entry& entry = entries_[entryIndex];
      if (!entry.valid) {
        continue;
      }
      const std::size_t start = HashProcessId(entry.processId) % kHashBucketCount;
      for (std::size_t probe = 0; probe < kHashBucketCount; ++probe) {
        const std::size_t bucketIndex = (start + probe) % kHashBucketCount;
        Bucket& bucket = buckets_[bucketIndex];
        if (bucket.occupied) {
          continue;
        }
        bucket.occupied = true;
        bucket.processId = entry.processId;
        bucket.entryIndex = entryIndex;
        break;
      }
    }
  }

  std::array<Entry, kCapacity> entries_{};
  std::array<Bucket, kHashBucketCount> buckets_{};
  std::size_t entryCount_ = 0;
  std::uint64_t nextGeneration_ = 1;
};

}  // namespace wallpaper
