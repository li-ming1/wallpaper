#include "wallpaper/video_path_matcher.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wallpaper {
namespace {

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) {
    return {};
  }
  std::wstring out(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), length);
  return out;
}

std::wstring NormalizeWindowsPathForCompare(const std::string& utf8Path) {
  if (utf8Path.empty()) {
    return {};
  }

  std::filesystem::path path = std::filesystem::path(Utf8ToWide(utf8Path));
  if (path.empty()) {
    return {};
  }
  if (path.is_relative()) {
    path = std::filesystem::absolute(path);
  }
  path = path.lexically_normal();

  std::wstring normalized = path.native();
  std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
  while (normalized.size() > 3 &&
         (normalized.back() == L'\\' || normalized.back() == L'/')) {
    normalized.pop_back();
  }
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](const wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return normalized;
}
#else
std::string NormalizePathForCompare(const std::string& utf8Path) {
  if (utf8Path.empty()) {
    return {};
  }

  std::filesystem::path path(utf8Path);
  if (path.empty()) {
    return {};
  }
  if (path.is_relative()) {
    path = std::filesystem::absolute(path);
  }
  path = path.lexically_normal();
  return path.generic_string();
}
#endif

}  // namespace

bool IsSameVideoPath(const std::string& lhsUtf8, const std::string& rhsUtf8) {
  if (lhsUtf8.empty() || rhsUtf8.empty()) {
    return lhsUtf8.empty() && rhsUtf8.empty();
  }

#ifdef _WIN32
  const std::wstring lhs = NormalizeWindowsPathForCompare(lhsUtf8);
  const std::wstring rhs = NormalizeWindowsPathForCompare(rhsUtf8);
  if (!lhs.empty() && !rhs.empty()) {
    return lhs == rhs;
  }

  std::string lhsFallback = lhsUtf8;
  std::string rhsFallback = rhsUtf8;
  std::replace(lhsFallback.begin(), lhsFallback.end(), '/', '\\');
  std::replace(rhsFallback.begin(), rhsFallback.end(), '/', '\\');
  std::transform(lhsFallback.begin(), lhsFallback.end(), lhsFallback.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  std::transform(rhsFallback.begin(), rhsFallback.end(), rhsFallback.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return lhsFallback == rhsFallback;
#else
  const std::string lhs = NormalizePathForCompare(lhsUtf8);
  const std::string rhs = NormalizePathForCompare(rhsUtf8);
  if (!lhs.empty() && !rhs.empty()) {
    return lhs == rhs;
  }
  std::string lhsFallback = lhsUtf8;
  std::string rhsFallback = rhsUtf8;
  std::transform(lhsFallback.begin(), lhsFallback.end(), lhsFallback.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  std::transform(rhsFallback.begin(), rhsFallback.end(), rhsFallback.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return lhsFallback == rhsFallback;
#endif
}

}  // namespace wallpaper
