#pragma once

#include <array>
#include <cstddef>

namespace wallpaper {

enum class SamplePublishFormat {
  kUnknown = 0,
  kNv12 = 1,
  kRgba = 2,
};

enum class SamplePublishStrategy {
  kUnknown = 0,
  kGpuDxgi = 1,
  kNv12Lock2D = 2,
  kNv12Contiguous = 3,
  kRgbaContiguous = 4,
};

struct SamplePublishCapabilities final {
  bool gpuZeroCopyActive = false;
  SamplePublishFormat format = SamplePublishFormat::kUnknown;
};

struct SamplePublishStrategyPlan final {
  std::array<SamplePublishStrategy, 3> strategies{
      SamplePublishStrategy::kUnknown,
      SamplePublishStrategy::kUnknown,
      SamplePublishStrategy::kUnknown,
  };
  std::size_t count = 0;
};

[[nodiscard]] constexpr bool IsSamplePublishStrategyCompatible(
    const SamplePublishStrategy strategy,
    const SamplePublishCapabilities capabilities) noexcept {
  if (capabilities.gpuZeroCopyActive) {
    return strategy == SamplePublishStrategy::kGpuDxgi;
  }
  if (capabilities.format == SamplePublishFormat::kNv12) {
    return strategy == SamplePublishStrategy::kNv12Lock2D ||
           strategy == SamplePublishStrategy::kNv12Contiguous;
  }
  if (capabilities.format == SamplePublishFormat::kRgba) {
    return strategy == SamplePublishStrategy::kRgbaContiguous;
  }
  return false;
}

[[nodiscard]] constexpr SamplePublishStrategy SelectPrimarySamplePublishStrategy(
    const SamplePublishCapabilities capabilities) noexcept {
  if (capabilities.gpuZeroCopyActive) {
    return SamplePublishStrategy::kGpuDxgi;
  }
  if (capabilities.format == SamplePublishFormat::kNv12) {
    return SamplePublishStrategy::kNv12Lock2D;
  }
  if (capabilities.format == SamplePublishFormat::kRgba) {
    return SamplePublishStrategy::kRgbaContiguous;
  }
  return SamplePublishStrategy::kUnknown;
}

[[nodiscard]] constexpr SamplePublishStrategy SelectSecondarySamplePublishStrategy(
    const SamplePublishCapabilities capabilities) noexcept {
  if (capabilities.gpuZeroCopyActive) {
    return SamplePublishStrategy::kUnknown;
  }
  if (capabilities.format == SamplePublishFormat::kNv12) {
    return SamplePublishStrategy::kNv12Contiguous;
  }
  return SamplePublishStrategy::kUnknown;
}

constexpr void AppendUniqueSamplePublishStrategy(
    SamplePublishStrategyPlan* const plan,
    const SamplePublishStrategy strategy) noexcept {
  if (plan == nullptr || strategy == SamplePublishStrategy::kUnknown) {
    return;
  }
  for (std::size_t index = 0; index < plan->count; ++index) {
    if (plan->strategies[index] == strategy) {
      return;
    }
  }
  if (plan->count < plan->strategies.size()) {
    plan->strategies[plan->count] = strategy;
    ++plan->count;
  }
}

class SamplePublishStrategyCache final {
 public:
  constexpr void Reset() noexcept { cached_ = SamplePublishStrategy::kUnknown; }

  constexpr void RememberSuccess(const SamplePublishStrategy strategy) noexcept {
    cached_ = strategy;
  }

  [[nodiscard]] constexpr SamplePublishStrategyPlan BuildPlan(
      const SamplePublishCapabilities capabilities) const noexcept {
    SamplePublishStrategyPlan plan;
    if (IsSamplePublishStrategyCompatible(cached_, capabilities)) {
      AppendUniqueSamplePublishStrategy(&plan, cached_);
    }
    AppendUniqueSamplePublishStrategy(&plan, SelectPrimarySamplePublishStrategy(capabilities));
    AppendUniqueSamplePublishStrategy(&plan, SelectSecondarySamplePublishStrategy(capabilities));
    return plan;
  }

 private:
  SamplePublishStrategy cached_ = SamplePublishStrategy::kUnknown;
};

}  // namespace wallpaper
