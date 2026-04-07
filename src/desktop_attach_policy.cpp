#include "wallpaper/desktop_attach_policy.h"

namespace wallpaper {

DesktopAttachPlan BuildDesktopAttachOrder(const bool hasWorkerW, const bool hasDefView) {
  DesktopAttachPlan order;
  if (hasWorkerW) {
    order.targets[order.count++] = DesktopAttachTarget::kWorkerW;
  }
  if (hasDefView) {
    order.targets[order.count++] = DesktopAttachTarget::kDefView;
  }
  return order;
}

}  // namespace wallpaper
