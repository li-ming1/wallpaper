#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
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

    const auto insertAt = std::find_if(retained_.begin(), retained_.end(),
                                       [&candidate](const MetricsShardCandidate& current) {
                                         return candidate.key > current.key;
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
    return std::any_of(retained_.begin(), retained_.end(),
                       [&key, &path](const MetricsShardCandidate& candidate) {
                         return candidate.key == key && candidate.path == path;
                       });
  }

 private:
  std::size_t keepCount_ = 0;
  std::vector<MetricsShardCandidate> retained_;
};

}  // namespace wallpaper
