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
  return plan;
}

}  // namespace wallpaper
