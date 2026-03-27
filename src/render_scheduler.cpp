#include "wallpaper/render_scheduler.h"

namespace wallpaper {
namespace {

RenderScheduler::Clock::duration BuildFrameInterval(const int fpsCap) {
  const int normalized = NormalizeFpsCap(fpsCap);
  return std::chrono::milliseconds(1000 / normalized);
}

}  // namespace

RenderScheduler::RenderScheduler(const int fpsCap)
    : fpsCap_(NormalizeFpsCap(fpsCap)), frameInterval_(BuildFrameInterval(fpsCap_)) {}

void RenderScheduler::SetFpsCap(const int fpsCap) {
  fpsCap_ = NormalizeFpsCap(fpsCap);
  frameInterval_ = BuildFrameInterval(fpsCap_);
}

int RenderScheduler::GetFpsCap() const noexcept { return fpsCap_; }

bool RenderScheduler::ShouldRender(const Clock::time_point now) {
  if (!hasLastFrame_) {
    hasLastFrame_ = true;
    lastFrameAt_ = now;
    return true;
  }

  if (now - lastFrameAt_ < frameInterval_) {
    return false;
  }

  lastFrameAt_ = now;
  return true;
}

void RenderScheduler::Reset() noexcept {
  hasLastFrame_ = false;
  lastFrameAt_ = Clock::time_point{};
}

}  // namespace wallpaper
