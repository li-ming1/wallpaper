#pragma once

#include <string>

namespace wallpaper {

bool IsDesktopContextClass(const std::wstring& className);
bool IsExplorerProcess(const std::wstring& processName);
bool ShouldTreatAsDesktopContext(const std::wstring& className,
                                 const std::wstring& processName);

}  // namespace wallpaper
