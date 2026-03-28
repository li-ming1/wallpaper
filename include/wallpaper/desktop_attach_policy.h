#pragma once

#include <vector>

namespace wallpaper {

enum class DesktopAttachTarget {
  kWorkerW,
  kDefView,
};

std::vector<DesktopAttachTarget> BuildDesktopAttachOrder(bool hasWorkerW,
                                                         bool hasDefView);

}  // namespace wallpaper
