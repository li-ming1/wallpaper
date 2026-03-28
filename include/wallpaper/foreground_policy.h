#pragma once

#include <string>

namespace wallpaper {

bool IsShellForegroundClass(const std::wstring& className);
bool ShouldTreatForegroundAsFullscreen(const std::wstring& className, bool coversMonitor,
                                       bool isVisible);
bool IsNearlyCoveringMonitor(int windowLeft, int windowTop, int windowRight, int windowBottom,
                             int monitorLeft, int monitorTop, int monitorRight, int monitorBottom,
                             int tolerancePx) noexcept;

}  // namespace wallpaper
