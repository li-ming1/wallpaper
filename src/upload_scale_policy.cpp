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

  // CPU 回退链路对 1080p+ 素材直接下探到 1/5 档位，优先守住硬预算。
  const bool allowFifthScale = sourceWidth >= 1920 && sourceHeight >= 1080 &&
                               sourceWidth % 5 == 0 && sourceHeight % 5 == 0;
  if (allowFifthScale) {
    plan.targetWidth = sourceWidth / 5;
    plan.targetHeight = sourceHeight / 5;
    plan.divisor = 5;
    return plan;
  }

  // 对无法整除 5 的更高分辨率素材仍保留 1/2 档位，避免非整数缩放带来额外复杂度。
  const bool allowHalfScale = sourceWidth >= 1920 && sourceHeight >= 1080 &&
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
