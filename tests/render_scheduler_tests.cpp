#include "wallpaper/render_scheduler.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(RenderScheduler_RespectsFpsCap30) {
  wallpaper::RenderScheduler scheduler(30);

  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};
  EXPECT_TRUE(scheduler.ShouldRender(t0));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 5ms));
  EXPECT_TRUE(scheduler.ShouldRender(t0 + 34ms));
}

TEST_CASE(RenderScheduler_SwitchCapTo60) {
  wallpaper::RenderScheduler scheduler(30);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};

  EXPECT_TRUE(scheduler.ShouldRender(t0));
  scheduler.SetFpsCap(60);
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 8ms));
  EXPECT_TRUE(scheduler.ShouldRender(t0 + 17ms));
}

TEST_CASE(RenderScheduler_TimeUntilNextRenderTracksInterval) {
  wallpaper::RenderScheduler scheduler(30);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};
  EXPECT_EQ(scheduler.TimeUntilNextRender(t0).count(), 0);

  EXPECT_TRUE(scheduler.ShouldRender(t0));
  const auto waitMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          scheduler.TimeUntilNextRender(t0 + 5ms));
  EXPECT_TRUE(waitMs.count() >= 28);
  EXPECT_TRUE(waitMs.count() <= 29);
}
