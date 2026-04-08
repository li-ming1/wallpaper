#pragma once

#include <expected>
#include <filesystem>

#include "wallpaper/config.h"

namespace wallpaper {
class AsyncFileWriter;

enum class ConfigStoreError {
  kFileMissing = 0,
  kFileOpenFailed = 1,
  kParseFailed = 2,
  kWriteFailed = 3,
};

class ConfigStore final {
 public:
  explicit ConfigStore(std::filesystem::path path, AsyncFileWriter* writer = nullptr);

  [[nodiscard]] std::expected<Config, ConfigStoreError> LoadExpected() const;
  [[nodiscard]] std::expected<void, ConfigStoreError> SaveExpected(const Config& config) const;
  [[nodiscard]] bool Exists() const;

 private:
  std::expected<void, ConfigStoreError> SaveExpectedInternal(const Config& config,
                                                             bool allowAsync) const;
  std::filesystem::path path_;
  AsyncFileWriter* writer_;
};

}  // namespace wallpaper
