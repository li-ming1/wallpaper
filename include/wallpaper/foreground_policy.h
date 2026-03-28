#pragma once

#include <string>

namespace wallpaper {

bool IsShellForegroundClass(const std::wstring& className);
bool ShouldTreatForegroundAsFullscreen(const std::wstring& className, bool coversMonitor,
                                       bool isVisible);

}  // namespace wallpaper
