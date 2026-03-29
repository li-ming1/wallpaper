#pragma once

namespace wallpaper {

struct TrayThreadStopDecision final {
  bool shouldPostQuit = false;
  bool shouldJoinWorker = false;
};

[[nodiscard]] constexpr TrayThreadStopDecision ComputeTrayThreadStopDecision(
    const bool wasRunning, const bool hasWorkerThreadId, const bool workerJoinable) noexcept {
  TrayThreadStopDecision decision;
  decision.shouldPostQuit = wasRunning && hasWorkerThreadId;
  decision.shouldJoinWorker = workerJoinable;
  return decision;
}

}  // namespace wallpaper
