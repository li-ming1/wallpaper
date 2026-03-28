#pragma once

#include <string>

namespace wallpaper {

// 仅当路径非空且指向现存文件时才启动视频渲染管线，避免无效配置触发遮盖层。
[[nodiscard]] bool ShouldActivateVideoPipeline(const std::string& videoPath);

// 首帧未就绪时禁止呈现，避免启动瞬间绘制回退色场导致“幕布闪现”。
[[nodiscard]] bool ShouldPresentFrame(bool hasDecodedFrame, bool hasLastPresentedFrame) noexcept;

}  // namespace wallpaper
