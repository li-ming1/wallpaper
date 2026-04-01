#pragma once

namespace wallpaper {

struct UploadScalePlan final {
  int targetWidth = 0;
  int targetHeight = 0;
  int divisor = 1;
};

// 质量优先路径下保持原始上传尺寸，不再为了 CPU fallback 主动压缩画面分辨率。
[[nodiscard]] UploadScalePlan SelectUploadScalePlanForCpuUpload(int sourceWidth,
                                                                int sourceHeight) noexcept;

}  // namespace wallpaper
