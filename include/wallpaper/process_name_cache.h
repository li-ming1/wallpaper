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
    std::size_t prevIndex = kInvalidIndex;
    std::size_t nextIndex = kInvalidIndex;
  };

  struct Bucket final {
    bool occupied = false;
    std::uint32_t processId = 0;
    std::size_t entryIndex = 0;
  };

  void InitializeEntryListsIfNeeded() noexcept {
    if (entryListsInitialized_) {
      return;
    }
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      Entry& entry = entries_[index];
      entry.prevIndex = kInvalidIndex;
      entry.nextIndex = (index + 1U < entries_.size()) ? (index + 1U) : kInvalidIndex;
    }
    freeEntryHead_ = entries_.empty() ? kInvalidIndex : 0U;
    lruHead_ = kInvalidIndex;
    lruTail_ = kInvalidIndex;
    entryListsInitialized_ = true;
  }

  void AttachEntryToLruTail(const std::size_t entryIndex) noexcept {
    if (entryIndex >= entries_.size()) {
      return;
    }
    Entry& entry = entries_[entryIndex];
    entry.prevIndex = lruTail_;
    entry.nextIndex = kInvalidIndex;
    if (lruTail_ != kInvalidIndex) {
      entries_[lruTail_].nextIndex = entryIndex;
    } else {
      lruHead_ = entryIndex;
    }
    lruTail_ = entryIndex;
  }

  void DetachEntryFromLru(const std::size_t entryIndex) noexcept {
    if (entryIndex >= entries_.size()) {
      return;
    }
    Entry& entry = entries_[entryIndex];
    const std::size_t prev = entry.prevIndex;
    const std::size_t next = entry.nextIndex;

    if (prev != kInvalidIndex) {
      entries_[prev].nextIndex = next;
    } else if (lruHead_ == entryIndex) {
      lruHead_ = next;
    }

    if (next != kInvalidIndex) {
      entries_[next].prevIndex = prev;
    } else if (lruTail_ == entryIndex) {
      lruTail_ = prev;
    }

    entry.prevIndex = kInvalidIndex;
    entry.nextIndex = kInvalidIndex;
  }

  void TouchLruEntry(const std::size_t entryIndex) noexcept {
    if (entryIndex >= entries_.size() || lruTail_ == entryIndex) {
      return;
    }
    DetachEntryFromLru(entryIndex);
    AttachEntryToLruTail(entryIndex);
  }

  [[nodiscard]] std::size_t AcquireEntryForStore() noexcept {
    if (freeEntryHead_ != kInvalidIndex) {
      const std::size_t entryIndex = freeEntryHead_;
      Entry& freeEntry = entries_[entryIndex];
      freeEntryHead_ = freeEntry.nextIndex;
      freeEntry.prevIndex = kInvalidIndex;
      freeEntry.nextIndex = kInvalidIndex;
      ++entryCount_;
      return entryIndex;
    }
    if (lruHead_ == kInvalidIndex) {
      return kInvalidIndex;
    }
    const std::size_t evictedIndex = lruHead_;
    DetachEntryFromLru(evictedIndex);
    return evictedIndex;
  }

  void Store(const std::uint32_t processId, const bool lookupOk, std::wstring processName) {
    InitializeEntryListsIfNeeded();
    const std::size_t existingBucket = FindBucketIndex(processId);
    if (existingBucket != kInvalidIndex) {
      const std::size_t existingEntry = buckets_[existingBucket].entryIndex;
      if (existingEntry < entries_.size()) {
        Entry& entry = entries_[existingEntry];
        if (entry.valid && entry.processId == processId) {
          entry.lookupOk = lookupOk;
          entry.processName = std::move(processName);
          TouchLruEntry(existingEntry);
          RebuildBuckets();
          return;
        }
      }
    }

    const std::size_t targetEntryIndex = AcquireEntryForStore();
    if (targetEntryIndex == kInvalidIndex) {
      return;
    }

    Entry& entry = entries_[targetEntryIndex];
    entry.processId = processId;
    entry.valid = true;
    entry.lookupOk = lookupOk;
    entry.processName = std::move(processName);
    AttachEntryToLruTail(targetEntryIndex);
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
  std::size_t freeEntryHead_ = kInvalidIndex;
  std::size_t lruHead_ = kInvalidIndex;
  std::size_t lruTail_ = kInvalidIndex;
  std::size_t entryCount_ = 0;
  bool entryListsInitialized_ = false;
};

}  // namespace wallpaper
