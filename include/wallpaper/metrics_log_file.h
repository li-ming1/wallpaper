#pragma once

#include <cstddef>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace wallpaper {
class AsyncFileWriter;

class MetricsLogFile final {
 public:
  using DateKeyProvider = std::function<std::string()>;

  MetricsLogFile(std::filesystem::path path, std::size_t maxBytes, std::string header,
                 std::size_t keepDays = 1, DateKeyProvider dateKeyProvider = {},
                 AsyncFileWriter* writer = nullptr);

 [[nodiscard]] bool EnsureReady() const;
  [[nodiscard]] bool Append(std::string_view line) const;

 private:
  [[nodiscard]] std::filesystem::path ActivePath() const;
  [[nodiscard]] bool EnsureReadyForPath(const std::filesystem::path& activePath) const;
  void MaybePruneShards(const std::filesystem::path& activePath) const;
  [[nodiscard]] bool RewriteWithHeader(const std::filesystem::path& path) const;
  void PruneShards(const std::filesystem::path& activePath) const;

  std::filesystem::path basePath_;
  std::size_t maxBytes_;
  std::string header_;
  std::size_t keepDays_;
  DateKeyProvider dateKeyProvider_;
  AsyncFileWriter* writer_;
  mutable std::filesystem::path lastPrunedActivePath_;
  mutable std::chrono::steady_clock::time_point lastPrunedAt_{};
};

}  // namespace wallpaper
