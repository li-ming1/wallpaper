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

TEST_CASE(RenderScheduler_RespectsFpsCap24) {
  wallpaper::RenderScheduler scheduler(24);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};

  EXPECT_EQ(scheduler.GetFpsCap(), 24);
  EXPECT_TRUE(scheduler.ShouldRender(t0));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 20ms));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 34ms));
  EXPECT_TRUE(scheduler.ShouldRender(t0 + 42ms));
}

TEST_CASE(RenderScheduler_RespectsFpsCap25) {
  wallpaper::RenderScheduler scheduler(25);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};

  EXPECT_EQ(scheduler.GetFpsCap(), 25);
  EXPECT_TRUE(scheduler.ShouldRender(t0));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 20ms));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 39ms));
  EXPECT_TRUE(scheduler.ShouldRender(t0 + 40ms));
}

TEST_CASE(RenderScheduler_RespectsFpsCap16) {
  wallpaper::RenderScheduler scheduler(16);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};

  EXPECT_EQ(scheduler.GetFpsCap(), 16);
  EXPECT_TRUE(scheduler.ShouldRender(t0));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 30ms));
  EXPECT_TRUE(!scheduler.ShouldRender(t0 + 55ms));
  EXPECT_TRUE(scheduler.ShouldRender(t0 + 63ms));
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

TEST_CASE(RenderScheduler_UsesSubMillisecondFrameIntervalAt60Fps) {
  wallpaper::RenderScheduler scheduler(60);
  const auto t0 = wallpaper::RenderScheduler::Clock::time_point{};
  EXPECT_TRUE(scheduler.ShouldRender(t0));

  const auto waitUs =
      std::chrono::duration_cast<std::chrono::microseconds>(
          scheduler.TimeUntilNextRender(t0 + 8ms))
          .count();
  EXPECT_TRUE(waitUs >= 8600);
  EXPECT_TRUE(waitUs <= 8700);
}
