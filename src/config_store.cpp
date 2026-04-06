#include "wallpaper/config_store.h"
#include "wallpaper/async_file_writer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace wallpaper {
namespace {

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void EnsureParentDirectory(const std::filesystem::path& path) {
  if (const auto parent = path.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
}

std::string EscapeJson(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        result += "\\\\";
        break;
      case '"':
        result += "\\\"";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += ch;
        break;
    }
  }
  return result;
}

std::string UnescapeJson(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '\\' && i + 1 < value.size()) {
      const char next = value[i + 1];
      switch (next) {
        case '\\':
          result.push_back('\\');
          ++i;
          continue;
        case '"':
          result.push_back('"');
          ++i;
          continue;
        case 'n':
          result.push_back('\n');
          ++i;
          continue;
        case 'r':
          result.push_back('\r');
          ++i;
          continue;
        case 't':
          result.push_back('\t');
          ++i;
          continue;
        default:
          break;
      }
    }
    result.push_back(ch);
  }
  return result;
}

std::size_t SkipWs(const std::string& json, std::size_t index) {
  while (index < json.size() &&
         std::isspace(static_cast<unsigned char>(json[index])) != 0) {
    ++index;
  }
  return index;
}

bool ExtractString(const std::string& json, const std::string& key, std::string* out) {
  // 只解析固定配置格式，避免引入重量级 JSON 依赖，优先启动时延和内存占用。
  const std::string needle = "\"" + key + "\"";
  const auto keyPos = json.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }

  auto pos = json.find(':', keyPos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  pos = SkipWs(json, pos + 1);
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  ++pos;

  std::string raw;
  bool escaped = false;
  while (pos < json.size()) {
    const char ch = json[pos++];
    if (!escaped && ch == '"') {
      *out = UnescapeJson(raw);
      return true;
    }
    if (!escaped && ch == '\\') {
      escaped = true;
      raw.push_back(ch);
      continue;
    }
    escaped = false;
    raw.push_back(ch);
  }
  return false;
}

bool ExtractBool(const std::string& json, const std::string& key, bool* out) {
  const std::string needle = "\"" + key + "\"";
  const auto keyPos = json.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }
  auto pos = json.find(':', keyPos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  pos = SkipWs(json, pos + 1);
  if (json.compare(pos, 4, "true") == 0) {
    *out = true;
    return true;
  }
  if (json.compare(pos, 5, "false") == 0) {
    *out = false;
    return true;
  }
  return false;
}

bool ContainsKey(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  return json.find(needle) != std::string::npos;
}

}  // namespace

ConfigStore::ConfigStore(std::filesystem::path path, AsyncFileWriter* writer)
    : path_(std::move(path)), writer_(writer) {}

std::expected<Config, ConfigStoreError> ConfigStore::LoadExpected() const {
  if (!Exists()) {
    return std::unexpected(ConfigStoreError::kFileMissing);
  }

  Config config;
  const std::string json = ReadAll(path_);
  if (json.empty()) {
    return std::unexpected(ConfigStoreError::kFileOpenFailed);
  }
  bool requiresRewrite = false;

  std::string value;
  if (ExtractString(json, "videoPath", &value)) {
    config.videoPath = value;
  }

  bool flag = false;
  if (ExtractBool(json, "autoStart", &flag)) {
    config.autoStart = flag;
  }
  if (ExtractBool(json, "pauseWhenNotDesktopContext", &flag)) {
    config.pauseWhenNotDesktopContext = flag;
  }

  if (ContainsKey(json, "pauseOnFullscreen") || ContainsKey(json, "pauseOnMaximized")) {
    requiresRewrite = true;
  }
  if (ContainsKey(json, "fpsCap") || ContainsKey(json, "renderCapMode")) {
    requiresRewrite = true;
  }
  if (ContainsKey(json, "adaptiveQuality")) {
    requiresRewrite = true;
  }
  if (ContainsKey(json, "frameLatencyWaitableMode")) {
    requiresRewrite = true;
  }
  if (ContainsKey(json, "codecPolicy")) {
    requiresRewrite = true;
  }

  if (requiresRewrite) {
    const auto saved = SaveExpectedInternal(config, /*allowAsync=*/false);
    if (!saved.has_value()) {
      return std::unexpected(saved.error());
    }
  }

  return config;
}

std::expected<void, ConfigStoreError> ConfigStore::SaveExpected(const Config& config) const {
  return SaveExpectedInternal(config, /*allowAsync=*/true);
}

std::expected<void, ConfigStoreError> ConfigStore::SaveExpectedInternal(
    const Config& config, const bool allowAsync) const {
  EnsureParentDirectory(path_);
  std::ostringstream out;
  out << "{\n";
  out << "  \"videoPath\": \"" << EscapeJson(config.videoPath) << "\",\n";
  out << "  \"autoStart\": " << (config.autoStart ? "true" : "false") << ",\n";
  out << "  \"pauseWhenNotDesktopContext\": "
      << (config.pauseWhenNotDesktopContext ? "true" : "false") << "\n";
  out << "}\n";
  const std::string json = out.str();

  if (allowAsync && writer_ != nullptr) {
    const bool enqueued =
        writer_->Enqueue(AsyncFileWriter::Task{path_, false, std::string(json)});
    if (enqueued) {
      return std::expected<void, ConfigStoreError>{};
    }
    // 异步队列不可用时回退同步写，确保配置持久化不受后台线程状态影响。
  }

  std::ofstream file(path_, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return std::unexpected(ConfigStoreError::kWriteFailed);
  }
  file.write(json.data(), static_cast<std::streamsize>(json.size()));
  if (!static_cast<bool>(file)) {
    return std::unexpected(ConfigStoreError::kWriteFailed);
  }
  return std::expected<void, ConfigStoreError>{};
}

Config ConfigStore::Load() const {
  const auto loaded = LoadExpected();
  if (loaded.has_value()) {
    return *loaded;
  }
  return {};
}

void ConfigStore::Save(const Config& config) const {
  (void)SaveExpected(config);
}

bool ConfigStore::Exists() const {
  std::error_code ec;
  return std::filesystem::exists(path_, ec) && !ec;
}

}  // namespace wallpaper
