#pragma once

namespace wallpaper {

enum class VideoColorSpace {
  kBt601Limited = 0,
  kBt709Limited = 1,
};

// 仅在目标视口严格大于源帧时启用更重的高质量上采样路径，避免对等比/缩小场景引入额外像素成本。
[[nodiscard]] bool ShouldUseHighQualityUpscale(int sourceWidth, int sourceHeight,
                                               int targetWidth, int targetHeight) noexcept;

// Windows/MF 常见 NV12 输出不总是携带显式矩阵元数据，这里用分辨率阈值做稳定启发式。
[[nodiscard]] VideoColorSpace SelectVideoColorSpace(int frameWidth, int frameHeight) noexcept;

}  // namespace wallpaper
