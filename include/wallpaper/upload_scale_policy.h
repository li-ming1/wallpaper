#pragma once

namespace wallpaper {

struct UploadScalePlan final {
  int targetWidth = 0;
  int targetHeight = 0;
  int divisor = 1;
};

// CPU 回退路径下优先压缩上传纹理尺寸，降低每帧上传带宽与纹理驻留。
[[nodiscard]] UploadScalePlan SelectUploadScalePlanForCpuUpload(int sourceWidth,
                                                                int sourceHeight) noexcept;

}  // namespace wallpaper
