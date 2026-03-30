#pragma once

#include <vector>

namespace wallpaper {

enum class DecodeOutputSubtype {
  kNv12 = 0,
  kArgb32 = 1,
  kRgb32 = 2,
};

[[nodiscard]] std::vector<DecodeOutputSubtype> BuildPreferredDecodeOutputSubtypes(
    bool d3dInteropEnabled, bool gpuNv12RenderingSupported);

}  // namespace wallpaper
