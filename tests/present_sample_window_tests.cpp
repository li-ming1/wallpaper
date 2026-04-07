#include "wallpaper/present_sample_window.h"

#include "test_support.h"

TEST_CASE(PresentSampleWindow_KeepsLatestFixedCapacitySamples) {
  wallpaper::PresentSampleWindow window;
  for (int index = 0; index < 70; ++index) {
    window.PushSample(static_cast<double>(index));
  }

  EXPECT_EQ(window.Size(), static_cast<std::size_t>(64));
  EXPECT_EQ(window.OldestSample(), 6.0);
  EXPECT_EQ(window.NewestSample(), 69.0);
}

TEST_CASE(PresentSampleWindow_TakeP95ClearsWindowAfterRead) {
  wallpaper::PresentSampleWindow window;
  for (int index = 1; index <= 10; ++index) {
    window.PushSample(static_cast<double>(index));
  }

  EXPECT_EQ(window.TakeP95AndClear(), 9.0);
  EXPECT_EQ(window.Size(), static_cast<std::size_t>(0));
}
