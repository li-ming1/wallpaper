#include "wallpaper/upload_scale_policy.h"

namespace wallpaper {

UploadScalePlan SelectUploadScalePlanForCpuUpload(const int sourceWidth,
                                                  const int sourceHeight) noexcept {
  UploadScalePlan plan;
  if (sourceWidth <= 0 || sourceHeight <= 0) {
    return plan;
  }

  plan.targetWidth = sourceWidth;
  plan.targetHeight = sourceHeight;
  plan.divisor = 1;

  // 仅对 1440p 及以上启用 1/2 上传采样，避免 1080p 常见素材被明显降质。
  const bool allowHalfScale = sourceWidth >= 2560 && sourceHeight >= 1440 &&
                              sourceWidth % 2 == 0 && sourceHeight % 2 == 0;
  if (!allowHalfScale) {
    return plan;
  }

  plan.targetWidth = sourceWidth / 2;
  plan.targetHeight = sourceHeight / 2;
  plan.divisor = 2;
  return plan;
}

}  // namespace wallpaper
