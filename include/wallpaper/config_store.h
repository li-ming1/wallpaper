#pragma once

#include <expected>
#include <filesystem>

#include "wallpaper/config.h"

namespace wallpaper {

enum class ConfigStoreError {
  kFileMissing = 0,
  kFileOpenFailed = 1,
  kParseFailed = 2,
  kWriteFailed = 3,
};

class ConfigStore final {
 public:
  explicit ConfigStore(std::filesystem::path path);

  [[nodiscard]] std::expected<Config, ConfigStoreError> LoadExpected() const;
  [[nodiscard]] std::expected<void, ConfigStoreError> SaveExpected(const Config& config) const;

  [[nodiscard]] Config Load() const;
  void Save(const Config& config) const;
  [[nodiscard]] bool Exists() const;

 private:
  std::filesystem::path path_;
};

}  // namespace wallpaper
