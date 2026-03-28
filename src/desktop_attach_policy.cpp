#include "wallpaper/desktop_attach_policy.h"

namespace wallpaper {

std::vector<DesktopAttachTarget> BuildDesktopAttachOrder(const bool hasWorkerW,
                                                         const bool hasDefView) {
  std::vector<DesktopAttachTarget> order;
  if (hasWorkerW) {
    order.push_back(DesktopAttachTarget::kWorkerW);
  }
  if (hasDefView) {
    order.push_back(DesktopAttachTarget::kDefView);
  }
  return order;
}

}  // namespace wallpaper
