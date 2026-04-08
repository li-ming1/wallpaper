#include "wallpaper/nearest_scale_stepper.h"

#include <vector>

#include "test_support.h"

namespace {

std::vector<int> CollectIndices(const int srcExtent, const int dstExtent) {
  wallpaper::NearestScaleStepper stepper(srcExtent, dstExtent);
  std::vector<int> indices;
  indices.reserve(static_cast<std::size_t>(dstExtent > 0 ? dstExtent : 0));
  for (int index = 0; index < dstExtent; ++index) {
    indices.push_back(stepper.CurrentSourceIndex());
    stepper.Advance();
  }
  return indices;
}

}  // namespace

TEST_CASE(NearestScaleStepper_DownscaleMatchesFloorMapping) {
  const std::vector<int> indices = CollectIndices(5, 3);
  EXPECT_EQ(indices.size(), static_cast<std::size_t>(3));
  EXPECT_EQ(indices[0], 0);
  EXPECT_EQ(indices[1], 1);
  EXPECT_EQ(indices[2], 3);
}

TEST_CASE(NearestScaleStepper_UpscaleRepeatsNearestSourceIndex) {
  const std::vector<int> indices = CollectIndices(3, 5);
  EXPECT_EQ(indices.size(), static_cast<std::size_t>(5));
  EXPECT_EQ(indices[0], 0);
  EXPECT_EQ(indices[1], 0);
  EXPECT_EQ(indices[2], 1);
  EXPECT_EQ(indices[3], 1);
  EXPECT_EQ(indices[4], 2);
}

TEST_CASE(NearestScaleStepper_InvalidExtentStaysAtZero) {
  const std::vector<int> indices = CollectIndices(0, 4);
  EXPECT_EQ(indices.size(), static_cast<std::size_t>(4));
  EXPECT_EQ(indices[0], 0);
  EXPECT_EQ(indices[1], 0);
  EXPECT_EQ(indices[2], 0);
  EXPECT_EQ(indices[3], 0);
}
