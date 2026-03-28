#include "wallpaper/config_store.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>
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

bool ExtractInt(const std::string& json, const std::string& key, int* out) {
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
  std::size_t end = pos;
  while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
    ++end;
  }
  if (end == pos) {
    return false;
  }
  *out = std::stoi(json.substr(pos, end - pos));
  return true;
}

}  // namespace

ConfigStore::ConfigStore(std::filesystem::path path) : path_(std::move(path)) {}

Config ConfigStore::Load() const {
  Config config;
  const std::string json = ReadAll(path_);
  if (json.empty()) {
    return config;
  }

  std::string value;
  if (ExtractString(json, "videoPath", &value)) {
    config.videoPath = value;
  }

  int fps = config.fpsCap;
  if (ExtractInt(json, "fpsCap", &fps)) {
    config.fpsCap = NormalizeFpsCap(fps);
  }

  bool flag = false;
  if (ExtractBool(json, "autoStart", &flag)) {
    config.autoStart = flag;
  }
  if (ExtractBool(json, "pauseOnFullscreen", &flag)) {
    config.pauseOnFullscreen = flag;
  }
  if (ExtractBool(json, "pauseOnMaximized", &flag)) {
    config.pauseOnMaximized = flag;
  }
  if (ExtractBool(json, "pauseWhenNotDesktopContext", &flag)) {
    config.pauseWhenNotDesktopContext = flag;
  }
  if (ExtractBool(json, "adaptiveQuality", &flag)) {
    config.adaptiveQuality = flag;
  }

  std::string codecValue;
  if (ExtractString(json, "codecPolicy", &codecValue)) {
    // 降级到 h264 作为安全默认值，避免非法配置导致启动失败。
    config.codecPolicy = codecValue == "h264+hevc" ? CodecPolicy::kH264PlusHevc
                                                   : CodecPolicy::kH264;
  }

  return config;
}

void ConfigStore::Save(const Config& config) const {
  EnsureParentDirectory(path_);
  std::ofstream out(path_, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return;
  }

  out << "{\n";
  out << "  \"videoPath\": \"" << EscapeJson(config.videoPath) << "\",\n";
  out << "  \"fpsCap\": " << NormalizeFpsCap(config.fpsCap) << ",\n";
  out << "  \"autoStart\": " << (config.autoStart ? "true" : "false") << ",\n";
  out << "  \"pauseOnFullscreen\": " << (config.pauseOnFullscreen ? "true" : "false")
      << ",\n";
  out << "  \"pauseOnMaximized\": " << (config.pauseOnMaximized ? "true" : "false")
      << ",\n";
  out << "  \"pauseWhenNotDesktopContext\": "
      << (config.pauseWhenNotDesktopContext ? "true" : "false") << ",\n";
  out << "  \"adaptiveQuality\": " << (config.adaptiveQuality ? "true" : "false") << ",\n";
  out << "  \"codecPolicy\": \""
      << (config.codecPolicy == CodecPolicy::kH264PlusHevc ? "h264+hevc" : "h264")
      << "\"\n";
  out << "}\n";
}

std::future<Config> ConfigStore::LoadAsync() const {
  return std::async(std::launch::async, [this]() { return Load(); });
}

std::future<void> ConfigStore::SaveAsync(Config config) const {
  return std::async(std::launch::async, [this, config = std::move(config)]() {
    Save(config);
  });
}

}  // namespace wallpaper
