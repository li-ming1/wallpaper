#include "wallpaper/metrics_log_file.h"
#include "wallpaper/async_file_writer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace wallpaper {
namespace {

std::string BuildLocalDateKey() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTm{};
  if (localtime_s(&localTm, &nowTime) != 0) {
    return {};
  }

  std::array<char, 9> buffer{};
  const std::size_t written = std::strftime(buffer.data(), buffer.size(), "%Y%m%d", &localTm);
  if (written != 8) {
    return {};
  }
  return std::string(buffer.data(), written);
}

bool IsDateKey(const std::string& text) {
  if (text.size() != 8) {
    return false;
  }
  return std::all_of(text.begin(), text.end(),
                     [](const char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
}

}  // namespace

MetricsLogFile::MetricsLogFile(std::filesystem::path path, const std::size_t maxBytes,
                               std::string header, const std::size_t keepDays,
                               DateKeyProvider dateKeyProvider, AsyncFileWriter* writer)
    : basePath_(std::move(path)),
      maxBytes_(std::max(maxBytes, header.size() + static_cast<std::size_t>(1))),
      header_(std::move(header)),
      keepDays_(std::max<std::size_t>(1, keepDays)),
      dateKeyProvider_(std::move(dateKeyProvider)),
      writer_(writer) {}

bool MetricsLogFile::EnsureReady() const {
  return EnsureReadyForPath(ActivePath());
}

bool MetricsLogFile::EnsureReadyForPath(const std::filesystem::path& activePath) const {
  std::error_code ec;
  const auto parent = activePath.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return false;
    }
  }

  if (!std::filesystem::exists(activePath, ec)) {
    if (!RewriteWithHeader(activePath)) {
      return false;
    }
    MaybePruneShards(activePath);
    return true;
  }
  if (ec) {
    return false;
  }

  const auto bytes = std::filesystem::file_size(activePath, ec);
  if (ec) {
    if (!RewriteWithHeader(activePath)) {
      return false;
    }
    MaybePruneShards(activePath);
    return true;
  }
  if (bytes == 0) {
    if (!RewriteWithHeader(activePath)) {
      return false;
    }
    MaybePruneShards(activePath);
    return true;
  }
  MaybePruneShards(activePath);
  return true;
}

bool MetricsLogFile::Append(const std::string_view line) const {
  if (header_.size() + line.size() > maxBytes_) {
    return false;
  }
  const std::filesystem::path activePath = ActivePath();
  if (!EnsureReadyForPath(activePath)) {
    return false;
  }

  std::error_code ec;
  const auto bytes = std::filesystem::file_size(activePath, ec);
  if (ec) {
    return false;
  }

  // 达到阈值直接回收成“仅表头”，确保长期运行文件体积稳定且写入路径简单。
  if (bytes + line.size() > maxBytes_ && !RewriteWithHeader(activePath)) {
    return false;
  }

  if (writer_ != nullptr) {
    return writer_->Enqueue(AsyncFileWriter::Task{activePath, true, std::string(line)});
  }

  std::ofstream out(activePath, std::ios::binary | std::ios::app);
  if (!out.is_open()) {
    return false;
  }
  out.write(line.data(), static_cast<std::streamsize>(line.size()));
  return static_cast<bool>(out);
}

void MetricsLogFile::MaybePruneShards(const std::filesystem::path& activePath) const {
  if (keepDays_ <= 1) {
    return;
  }
  constexpr std::chrono::minutes kPruneInterval(10);

  const auto now = std::chrono::steady_clock::now();
  const bool activePathChanged = activePath != lastPrunedActivePath_;
  const bool pruneOverdue = lastPrunedAt_ == std::chrono::steady_clock::time_point{} ||
                            now < lastPrunedAt_ ||
                            (now - lastPrunedAt_) >= kPruneInterval;
  if (!activePathChanged && !pruneOverdue) {
    return;
  }

  PruneShards(activePath);
  lastPrunedActivePath_ = activePath;
  lastPrunedAt_ = now;
}

std::filesystem::path MetricsLogFile::ActivePath() const {
  if (keepDays_ <= 1) {
    return basePath_;
  }

  const std::string key =
      dateKeyProvider_ ? dateKeyProvider_() : BuildLocalDateKey();
  if (!IsDateKey(key)) {
    return basePath_;
  }

  const std::string stem = basePath_.stem().string();
  const std::string ext = basePath_.extension().string();
  const std::string shardName =
      (stem.empty() ? std::string("metrics") : stem) + "_" + key + ext;
  const auto parent = basePath_.parent_path();
  return parent.empty() ? std::filesystem::path(shardName) : parent / shardName;
}

bool MetricsLogFile::RewriteWithHeader(const std::filesystem::path& path) const {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out.write(header_.data(), static_cast<std::streamsize>(header_.size()));
  return static_cast<bool>(out);
}

void MetricsLogFile::PruneShards(const std::filesystem::path& activePath) const {
  if (keepDays_ <= 1) {
    return;
  }

  const std::string stem = basePath_.stem().string();
  const std::string ext = basePath_.extension().string();
  const std::string prefix = (stem.empty() ? std::string("metrics") : stem) + "_";
  const auto dir = basePath_.parent_path().empty() ? std::filesystem::current_path()
                                                   : basePath_.parent_path();

  std::vector<std::pair<std::string, std::filesystem::path>> shards;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    if (name.size() < prefix.size() + 8 + ext.size()) {
      continue;
    }
    if (name.rfind(prefix, 0) != 0) {
      continue;
    }
    if (!ext.empty() && entry.path().extension() != ext) {
      continue;
    }
    const std::size_t keyBegin = prefix.size();
    const std::string key = name.substr(keyBegin, 8);
    if (!IsDateKey(key)) {
      continue;
    }
    shards.push_back(std::make_pair(key, entry.path()));
  }

  std::sort(shards.begin(), shards.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  for (std::size_t i = keepDays_; i < shards.size(); ++i) {
    if (shards[i].second == activePath) {
      continue;
    }
    std::error_code removeEc;
    std::filesystem::remove(shards[i].second, removeEc);
  }
}

}  // namespace wallpaper
