#pragma once

#include "wallpaper/monitor_layout_policy.h"

namespace wallpaper {

struct MonitorRectSnapshot final {
  DisplayRect virtualDesktop{};
  DisplayRectPlan monitors{};
};

[[nodiscard]] MonitorRectSnapshot QueryMonitorRectSnapshotCached();

}  // namespace wallpaper
