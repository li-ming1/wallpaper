#include "wallpaper/source_frame_rate_policy.h"

#include "test_support.h"

TEST_CASE(SourceFrameRatePolicy_Detects24FpsAfterStableSamples) {
  wallpaper::SourceFrameRateState state;
  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(1'000'000 + i * 416'666,
                                                          1'416'666 + i * 416'666, &state);
    EXPECT_TRUE(fps >= 24);
  }

  EXPECT_EQ(state.sourceFps, 24);
}

TEST_CASE(SourceFrameRatePolicy_Detects25FpsAfterStableSamples) {
  wallpaper::SourceFrameRateState state;
  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(2'000'000 + i * 400'000,
                                                          2'400'000 + i * 400'000, &state);
    EXPECT_TRUE(fps >= 25);
  }

  EXPECT_EQ(state.sourceFps, 25);
}

TEST_CASE(SourceFrameRatePolicy_Detects30FpsAfterStableSamples) {
  wallpaper::SourceFrameRateState state;
  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(3'000'000 + i * 333'333,
                                                          3'333'333 + i * 333'333, &state);
    EXPECT_TRUE(fps >= 30);
  }

  EXPECT_EQ(state.sourceFps, 30);
}

TEST_CASE(SourceFrameRatePolicy_Detects60FpsAfterStableSamples) {
  wallpaper::SourceFrameRateState state;
  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(4'000'000 + i * 166'666,
                                                          4'166'666 + i * 166'666, &state);
    EXPECT_TRUE(fps >= 30);
  }

  EXPECT_EQ(state.sourceFps, 60);
}

TEST_CASE(SourceFrameRatePolicy_UpdatesWhenCadenceChanges) {
  wallpaper::SourceFrameRateState state;
  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(10'000'000 + i * 166'666,
                                                          10'166'666 + i * 166'666, &state);
    (void)fps;
  }
  EXPECT_EQ(state.sourceFps, 60);

  for (int i = 0; i < 4; ++i) {
    const int fps = wallpaper::UpdateSourceFrameRateState(20'000'000 + i * 333'333,
                                                          20'333'333 + i * 333'333, &state);
    (void)fps;
  }
  EXPECT_EQ(state.sourceFps, 30);
}

TEST_CASE(SourceFrameRatePolicy_IgnoresInvalidTimestampOrder) {
  wallpaper::SourceFrameRateState state;
  state.sourceFps = 25;

  const int fps = wallpaper::UpdateSourceFrameRateState(5'000'000, 4'999'999, &state);

  EXPECT_EQ(fps, 25);
  EXPECT_EQ(state.sourceFps, 25);
}
