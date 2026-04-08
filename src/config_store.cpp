#include "wallpaper/config_store.h"
#include "wallpaper/async_file_writer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
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

bool ParseJsonString(const std::string& json, std::size_t* const pos, std::string* const out) {
  if (pos == nullptr || *pos >= json.size() || json[*pos] != '"') {
    return false;
  }
  ++(*pos);

  std::string raw;
  bool escaped = false;
  while (*pos < json.size()) {
    const char ch = json[*pos];
    ++(*pos);
    if (!escaped && ch == '"') {
      if (out != nullptr) {
        *out = UnescapeJson(raw);
      }
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

bool ParseJsonBool(const std::string& json, std::size_t* const pos, bool* const out) {
  if (pos == nullptr || *pos >= json.size()) {
    return false;
  }
  if (json.compare(*pos, 4, "true") == 0) {
    if (out != nullptr) {
      *out = true;
    }
    *pos += 4;
    return true;
  }
  if (json.compare(*pos, 5, "false") == 0) {
    if (out != nullptr) {
      *out = false;
    }
    *pos += 5;
    return true;
  }
  return false;
}

bool ParseJsonNull(const std::string& json, std::size_t* const pos) {
  if (pos == nullptr || *pos >= json.size()) {
    return false;
  }
  if (json.compare(*pos, 4, "null") != 0) {
    return false;
  }
  *pos += 4;
  return true;
}

bool ParseJsonNumber(const std::string& json, std::size_t* const pos) {
  if (pos == nullptr || *pos >= json.size()) {
    return false;
  }

  std::size_t index = *pos;
  if (json[index] == '-') {
    ++index;
    if (index >= json.size()) {
      return false;
    }
  }

  if (json[index] == '0') {
    ++index;
  } else if (std::isdigit(static_cast<unsigned char>(json[index])) != 0) {
    while (index < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[index])) != 0) {
      ++index;
    }
  } else {
    return false;
  }

  if (index < json.size() && json[index] == '.') {
    ++index;
    const std::size_t fractionBegin = index;
    while (index < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[index])) != 0) {
      ++index;
    }
    if (index == fractionBegin) {
      return false;
    }
  }

  if (index < json.size() && (json[index] == 'e' || json[index] == 'E')) {
    ++index;
    if (index < json.size() && (json[index] == '+' || json[index] == '-')) {
      ++index;
    }
    const std::size_t exponentBegin = index;
    while (index < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[index])) != 0) {
      ++index;
    }
    if (index == exponentBegin) {
      return false;
    }
  }

  *pos = index;
  return true;
}

bool SkipJsonValue(const std::string& json, std::size_t* const pos) {
  if (pos == nullptr || *pos >= json.size()) {
    return false;
  }
  const char ch = json[*pos];
  if (ch == '"') {
    return ParseJsonString(json, pos, nullptr);
  }
  if (ch == '{' || ch == '[') {
    std::string stack;
    stack.push_back(ch == '{' ? '}' : ']');
    ++(*pos);
    bool inString = false;
    bool escaped = false;
    while (*pos < json.size()) {
      const char current = json[*pos];
      ++(*pos);
      if (inString) {
        if (!escaped && current == '"') {
          inString = false;
        } else {
          escaped = !escaped && current == '\\';
        }
        continue;
      }
      if (current == '"') {
        inString = true;
        escaped = false;
        continue;
      }
      if (current == '{') {
        stack.push_back('}');
        continue;
      }
      if (current == '[') {
        stack.push_back(']');
        continue;
      }
      if (!stack.empty() && current == stack.back()) {
        stack.pop_back();
        if (stack.empty()) {
          return true;
        }
      }
    }
    return false;
  }
  bool parsedBool = false;
  if (ParseJsonBool(json, pos, &parsedBool)) {
    return true;
  }
  if (ParseJsonNull(json, pos)) {
    return true;
  }
  return ParseJsonNumber(json, pos);
}

struct ParsedConfigFields final {
  std::optional<std::string> videoPath;
  std::optional<PlaybackProfile> playbackProfile;
  std::optional<bool> autoStart;
  std::optional<bool> pauseWhenNotDesktopContext;
  std::optional<bool> debugMetrics;
};

bool ParseTopLevelConfig(const std::string& json, ParsedConfigFields* const out) {
  if (out == nullptr) {
    return false;
  }
  std::size_t pos = SkipWs(json, 0);
  if (pos >= json.size() || json[pos] != '{') {
    return false;
  }
  ++pos;

  for (;;) {
    pos = SkipWs(json, pos);
    if (pos >= json.size()) {
      return false;
    }
    if (json[pos] == '}') {
      ++pos;
      return SkipWs(json, pos) == json.size();
    }

    std::string key;
    if (!ParseJsonString(json, &pos, &key)) {
      return false;
    }
    pos = SkipWs(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
      return false;
    }
    ++pos;
    pos = SkipWs(json, pos);
    if (pos >= json.size()) {
      return false;
    }

    if (key == "videoPath") {
      std::string value;
      if (!ParseJsonString(json, &pos, &value)) {
        return false;
      }
      out->videoPath = std::move(value);
    } else if (key == "playbackProfile") {
      std::string value;
      if (!ParseJsonString(json, &pos, &value)) {
        return false;
      }
      PlaybackProfile parsedProfile = PlaybackProfile::kBalanced;
      if (TryParsePlaybackProfile(value, &parsedProfile)) {
        out->playbackProfile = parsedProfile;
      }
    } else if (key == "autoStart") {
      bool value = false;
      if (!ParseJsonBool(json, &pos, &value)) {
        return false;
      }
      out->autoStart = value;
    } else if (key == "pauseWhenNotDesktopContext") {
      bool value = false;
      if (!ParseJsonBool(json, &pos, &value)) {
        return false;
      }
      out->pauseWhenNotDesktopContext = value;
    } else if (key == "debugMetrics") {
      bool value = false;
      if (!ParseJsonBool(json, &pos, &value)) {
        return false;
      }
      out->debugMetrics = value;
    } else {
      if (!SkipJsonValue(json, &pos)) {
        return false;
      }
    }

    pos = SkipWs(json, pos);
    if (pos >= json.size()) {
      return false;
    }
    if (json[pos] == ',') {
      ++pos;
      continue;
    }
    if (json[pos] == '}') {
      ++pos;
      return SkipWs(json, pos) == json.size();
    }
    return false;
  }
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

  ParsedConfigFields parsed;
  if (!ParseTopLevelConfig(json, &parsed)) {
    return std::unexpected(ConfigStoreError::kParseFailed);
  }
  if (parsed.videoPath.has_value()) {
    config.videoPath = *parsed.videoPath;
  }
  if (parsed.playbackProfile.has_value()) {
    config.playbackProfile = *parsed.playbackProfile;
  }
  if (parsed.autoStart.has_value()) {
    config.autoStart = *parsed.autoStart;
  }
  if (parsed.pauseWhenNotDesktopContext.has_value()) {
    config.pauseWhenNotDesktopContext = *parsed.pauseWhenNotDesktopContext;
  }
  if (parsed.debugMetrics.has_value()) {
    config.debugMetrics = *parsed.debugMetrics;
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
  out << "  \"playbackProfile\": \"" << ToConfigString(config.playbackProfile) << "\",\n";
  out << "  \"autoStart\": " << (config.autoStart ? "true" : "false") << ",\n";
  out << "  \"debugMetrics\": " << (config.debugMetrics ? "true" : "false") << ",\n";
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
