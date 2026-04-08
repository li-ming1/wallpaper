#include "wallpaper/metrics_log_file.h"
#include "wallpaper/async_file_writer.h"
#include "wallpaper/metrics_shard_retain_set.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <unordered_set>
#include <system_error>
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

bool TryParseMetricsShardCandidate(const std::filesystem::directory_entry& entry,
                                   const std::string& prefix, const std::string& ext,
                                   MetricsShardCandidate* const out) {
  if (out == nullptr || !entry.is_regular_file()) {
    return false;
  }
  const auto name = entry.path().filename().string();
  if (name.size() < prefix.size() + 8 + ext.size()) {
    return false;
  }
  if (name.rfind(prefix, 0) != 0) {
    return false;
  }
  if (!ext.empty() && entry.path().extension() != ext) {
    return false;
  }
  const std::size_t keyBegin = prefix.size();
  const std::string key = name.substr(keyBegin, 8);
  if (!IsDateKey(key)) {
    return false;
  }
  out->key = key;
  out->path = entry.path();
  return true;
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

void MetricsLogFile::SetWriter(AsyncFileWriter* const writer) noexcept { writer_ = writer; }

bool MetricsLogFile::EnsureReady() const {
  const std::filesystem::path activePath = ActivePath();
  return RefreshActiveShardSizeCache(activePath);
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
  if (!cachedActivePathBytesKnown_ || cachedActivePath_ != activePath) {
    if (!RefreshActiveShardSizeCache(activePath)) {
      return false;
    }
  }

  if (cachedActivePathBytes_ + line.size() > maxBytes_ && !RewriteWithHeader(activePath)) {
    InvalidateActiveShardSizeCache();
    return false;
  }
  if (cachedActivePathBytes_ + line.size() > maxBytes_) {
    cachedActivePathBytes_ = header_.size();
  }

  bool appended = false;
  if (writer_ != nullptr) {
    appended = writer_->Enqueue(AsyncFileWriter::Task{activePath, true, std::string(line)});
  } else {
    std::ofstream out(activePath, std::ios::binary | std::ios::app);
    if (!out.is_open()) {
      InvalidateActiveShardSizeCache();
      return false;
    }
    out.write(line.data(), static_cast<std::streamsize>(line.size()));
    appended = static_cast<bool>(out);
  }
  if (!appended) {
    InvalidateActiveShardSizeCache();
    return false;
  }
  cachedActivePathBytes_ += line.size();
  return true;
}

bool MetricsLogFile::RefreshActiveShardSizeCache(const std::filesystem::path& activePath) const {
  if (!EnsureReadyForPath(activePath)) {
    InvalidateActiveShardSizeCache();
    return false;
  }
  std::error_code ec;
  const auto bytes = std::filesystem::file_size(activePath, ec);
  if (ec) {
    InvalidateActiveShardSizeCache();
    return false;
  }
  cachedActivePath_ = activePath;
  cachedActivePathBytes_ = bytes;
  cachedActivePathBytesKnown_ = true;
  return true;
}

void MetricsLogFile::InvalidateActiveShardSizeCache() const noexcept {
  cachedActivePath_.clear();
  cachedActivePathBytes_ = 0;
  cachedActivePathBytesKnown_ = false;
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

  MetricsShardRetainSet retainSet(keepDays_);
  std::vector<MetricsShardCandidate> candidates;
  candidates.reserve(keepDays_ + 4U);
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      continue;
    }
    MetricsShardCandidate candidate;
    if (TryParseMetricsShardCandidate(entry, prefix, ext, &candidate)) {
      retainSet.Consider(candidate);
      candidates.push_back(std::move(candidate));
    }
  }

  std::unordered_set<std::filesystem::path> retainedPaths;
  retainedPaths.reserve(retainSet.Items().size() + static_cast<std::size_t>(1));
  retainedPaths.insert(activePath);
  for (const MetricsShardCandidate& retained : retainSet.Items()) {
    retainedPaths.insert(retained.path);
  }

  for (const MetricsShardCandidate& candidate : candidates) {
    if (retainedPaths.find(candidate.path) != retainedPaths.end()) {
      continue;
    }
    std::error_code removeEc;
    std::filesystem::remove(candidate.path, removeEc);
  }
}

}  // namespace wallpaper
