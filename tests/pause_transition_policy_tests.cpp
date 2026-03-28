#include "wallpaper/pause_transition_policy.h"

#include <chrono>

#include "test_support.h"

using namespace std::chrono_literals;

TEST_CASE(PauseTransitionPolicy_EnterPauseAfterDelay) {
  wallpaper::PauseTransitionState state;
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(true, t0, 150ms, 220ms, &state));
  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(true, t0 + 120ms, 150ms, 220ms, &state));
  EXPECT_TRUE(wallpaper::UpdatePauseTransition(true, t0 + 151ms, 150ms, 220ms, &state));
}

TEST_CASE(PauseTransitionPolicy_ExitPauseAfterDelay) {
  wallpaper::PauseTransitionState state;
  const auto t0 = std::chrono::steady_clock::time_point{};
  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(true, t0, 100ms, 200ms, &state));
  EXPECT_TRUE(wallpaper::UpdatePauseTransition(true, t0 + 120ms, 100ms, 200ms, &state));

  EXPECT_TRUE(wallpaper::UpdatePauseTransition(false, t0 + 130ms, 100ms, 200ms, &state));
  EXPECT_TRUE(wallpaper::UpdatePauseTransition(false, t0 + 300ms, 100ms, 200ms, &state));
  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(false, t0 + 331ms, 100ms, 200ms, &state));
}

TEST_CASE(PauseTransitionPolicy_FlapBeforeEnterKeepsRunning) {
  wallpaper::PauseTransitionState state;
  const auto t0 = std::chrono::steady_clock::time_point{};

  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(true, t0, 150ms, 220ms, &state));
  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(false, t0 + 70ms, 150ms, 220ms, &state));
  EXPECT_TRUE(!wallpaper::UpdatePauseTransition(true, t0 + 110ms, 150ms, 220ms, &state));
}
