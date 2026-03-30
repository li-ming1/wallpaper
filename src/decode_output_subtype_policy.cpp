#include "wallpaper/decode_output_subtype_policy.h"

namespace wallpaper {

std::vector<DecodeOutputSubtype> BuildPreferredDecodeOutputSubtypes(
    const bool d3dInteropEnabled, const bool gpuNv12RenderingSupported) {
  if (!d3dInteropEnabled) {
    return {DecodeOutputSubtype::kNv12, DecodeOutputSubtype::kRgb32};
  }
  if (gpuNv12RenderingSupported) {
    return {DecodeOutputSubtype::kNv12, DecodeOutputSubtype::kArgb32,
            DecodeOutputSubtype::kRgb32};
  }
  return {DecodeOutputSubtype::kArgb32, DecodeOutputSubtype::kRgb32};
}

}  // namespace wallpaper
