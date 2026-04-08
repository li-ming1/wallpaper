#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wallpaper {

struct MetricsShardCandidate final {
  std::string key;
  std::filesystem::path path;
};

class MetricsShardRetainSet final {
 public:
  explicit MetricsShardRetainSet(const std::size_t keepCount) : keepCount_(keepCount) {
    retained_.reserve(keepCount_);
  }

  void Consider(MetricsShardCandidate candidate) {
    if (keepCount_ == 0) {
      return;
    }

    const auto insertAt = std::lower_bound(
        retained_.begin(), retained_.end(), candidate.key,
        [](const MetricsShardCandidate& current, const std::string& key) {
          return current.key > key;
        });
    if (insertAt == retained_.end() && retained_.size() >= keepCount_) {
      return;
    }

    retained_.insert(insertAt, std::move(candidate));
    if (retained_.size() > keepCount_) {
      retained_.pop_back();
    }
  }

  [[nodiscard]] bool Contains(const std::string_view key,
                              const std::filesystem::path& path) const {
    const std::string lookupKey(key);
    auto it = std::lower_bound(
        retained_.begin(), retained_.end(), lookupKey,
        [](const MetricsShardCandidate& current, const std::string& currentKey) {
          return current.key > currentKey;
        });
    while (it != retained_.end() && it->key == lookupKey) {
      if (it->path == path) {
        return true;
      }
      ++it;
    }
    return false;
  }

  [[nodiscard]] std::span<const MetricsShardCandidate> Items() const {
    return retained_;
  }

 private:
  std::size_t keepCount_ = 0;
  std::vector<MetricsShardCandidate> retained_;
};

}  // namespace wallpaper
