#pragma once

#include <array>
#include <cstddef>

namespace wallpaper {

enum class DesktopAttachTarget {
  kWorkerW,
  kDefView,
};

struct DesktopAttachPlan final {
  std::array<DesktopAttachTarget, 2> targets{
      DesktopAttachTarget::kWorkerW,
      DesktopAttachTarget::kWorkerW,
  };
  std::size_t count = 0;

  [[nodiscard]] bool empty() const noexcept { return count == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return count; }
  [[nodiscard]] const DesktopAttachTarget& operator[](const std::size_t index) const noexcept {
    return targets[index];
  }
  [[nodiscard]] const DesktopAttachTarget* begin() const noexcept { return targets.data(); }
  [[nodiscard]] const DesktopAttachTarget* end() const noexcept { return targets.data() + count; }
};

DesktopAttachPlan BuildDesktopAttachOrder(bool hasWorkerW, bool hasDefView);

}  // namespace wallpaper
