#pragma once

#include <filesystem>
#include <future>

#include "wallpaper/config.h"

namespace wallpaper {

class ConfigStore final {
 public:
  explicit ConfigStore(std::filesystem::path path);

  [[nodiscard]] Config Load() const;
  void Save(const Config& config) const;

  [[nodiscard]] std::future<Config> LoadAsync() const;
  [[nodiscard]] std::future<void> SaveAsync(Config config) const;

 private:
  std::filesystem::path path_;
};

}  // namespace wallpaper
