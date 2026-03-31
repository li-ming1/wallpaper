#include "wallpaper/decode_output_policy.h"

#include "test_support.h"

TEST_CASE(DecodeOutputPolicy_AdaptiveOffKeepsDesktopHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = false;
  options.cpuFallbackPath = true;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_GpuPathKeepsDesktopHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = false;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 3840U);
  EXPECT_EQ(hint.height, 2160U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathCapsPixelsAt540pInNormalPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 960U);
  EXPECT_EQ(hint.height, 540U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathDropsTo432pAtMediumPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 1;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 768U);
  EXPECT_EQ(hint.height, 432U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathDropsTo360pAtHighPressure) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 3840;
  options.desktopHeight = 2160;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 2;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 640U);
  EXPECT_EQ(hint.height, 360U);
}

TEST_CASE(DecodeOutputPolicy_CpuPathKeepsSmallFrames) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 960;
  options.desktopHeight = 540;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 2;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 960U);
  EXPECT_EQ(hint.height, 540U);
}

TEST_CASE(DecodeOutputPolicy_ZeroInputReturnsZero) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 0;
  options.desktopHeight = 0;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;

  const wallpaper::DecodeOutputHint hint = wallpaper::SelectDecodeOutputHint(options);
  EXPECT_EQ(hint.width, 0U);
  EXPECT_EQ(hint.height, 0U);
}

TEST_CASE(DecodeOutputPolicy_RetryVideoProcessingWhenCpuFallbackNegotiationExceedsHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  EXPECT_TRUE(
      wallpaper::ShouldRetryDecodeOpenWithVideoProcessing(options, 1920, 1080));
}

TEST_CASE(DecodeOutputPolicy_NoRetryWhenCpuFallbackNegotiationHonorsHint) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  EXPECT_TRUE(
      !wallpaper::ShouldRetryDecodeOpenWithVideoProcessing(options, 960, 540));
}

TEST_CASE(DecodeOutputPolicy_NoRetryWhenAdaptiveDisabledOrGpuPath) {
  wallpaper::DecodeOutputOptions adaptiveOff;
  adaptiveOff.desktopWidth = 1920;
  adaptiveOff.desktopHeight = 1080;
  adaptiveOff.adaptiveQualityEnabled = false;
  adaptiveOff.cpuFallbackPath = true;
  EXPECT_TRUE(
      !wallpaper::ShouldRetryDecodeOpenWithVideoProcessing(adaptiveOff, 1920, 1080));

  wallpaper::DecodeOutputOptions gpuPath;
  gpuPath.desktopWidth = 1920;
  gpuPath.desktopHeight = 1080;
  gpuPath.adaptiveQualityEnabled = true;
  gpuPath.cpuFallbackPath = false;
  EXPECT_TRUE(
      !wallpaper::ShouldRetryDecodeOpenWithVideoProcessing(gpuPath, 1920, 1080));
}

TEST_CASE(DecodeOutputPolicy_EnablesAdvancedVideoProcessingForAdaptiveCpuSoftwarePath) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 1;

  EXPECT_TRUE(wallpaper::ShouldEnableAdvancedVideoProcessing(options, true));
}

TEST_CASE(DecodeOutputPolicy_DisablesAdvancedVideoProcessingWhenConditionsNotMet) {
  wallpaper::DecodeOutputOptions adaptiveOff;
  adaptiveOff.desktopWidth = 1920;
  adaptiveOff.desktopHeight = 1080;
  adaptiveOff.adaptiveQualityEnabled = false;
  adaptiveOff.cpuFallbackPath = true;
  EXPECT_TRUE(!wallpaper::ShouldEnableAdvancedVideoProcessing(adaptiveOff, true));

  wallpaper::DecodeOutputOptions gpuPath;
  gpuPath.desktopWidth = 1920;
  gpuPath.desktopHeight = 1080;
  gpuPath.adaptiveQualityEnabled = true;
  gpuPath.cpuFallbackPath = false;
  EXPECT_TRUE(!wallpaper::ShouldEnableAdvancedVideoProcessing(gpuPath, true));

  wallpaper::DecodeOutputOptions softwareOff;
  softwareOff.desktopWidth = 1920;
  softwareOff.desktopHeight = 1080;
  softwareOff.adaptiveQualityEnabled = true;
  softwareOff.cpuFallbackPath = true;
  EXPECT_TRUE(!wallpaper::ShouldEnableAdvancedVideoProcessing(softwareOff, false));

  wallpaper::DecodeOutputOptions zeroSize;
  zeroSize.desktopWidth = 0;
  zeroSize.desktopHeight = 1080;
  zeroSize.adaptiveQualityEnabled = true;
  zeroSize.cpuFallbackPath = true;
  EXPECT_TRUE(!wallpaper::ShouldEnableAdvancedVideoProcessing(zeroSize, true));
}

TEST_CASE(DecodeOutputPolicy_PreservesD3DInteropOnAdaptiveCpuRetryWhenHardwarePreferred) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  EXPECT_TRUE(wallpaper::ShouldPreserveD3DInteropOnVideoProcessingRetry(options, true));
}

TEST_CASE(DecodeOutputPolicy_DoesNotForceD3DInteropWhenHardwareNotPreferred) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;
  options.longRunLoadLevel = 0;

  EXPECT_TRUE(!wallpaper::ShouldPreserveD3DInteropOnVideoProcessingRetry(options, false));
}

TEST_CASE(DecodeOutputPolicy_RequiresD3DInteropBindingWhenHardwarePreferredInAdaptiveCpuPath) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;

  EXPECT_TRUE(wallpaper::ShouldRequireD3DInteropBinding(options, true, false));
}

TEST_CASE(DecodeOutputPolicy_RequiresD3DInteropBindingWhenHardwareStrictlyRequired) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = true;
  options.cpuFallbackPath = true;

  EXPECT_TRUE(wallpaper::ShouldRequireD3DInteropBinding(options, false, true));
}

TEST_CASE(DecodeOutputPolicy_DoesNotRequireD3DInteropBindingWhenNotHardwarePath) {
  wallpaper::DecodeOutputOptions options;
  options.desktopWidth = 1920;
  options.desktopHeight = 1080;
  options.adaptiveQualityEnabled = false;
  options.cpuFallbackPath = true;

  EXPECT_TRUE(!wallpaper::ShouldRequireD3DInteropBinding(options, false, false));
}

TEST_CASE(DecodeOutputPolicy_DisablesLegacyVideoProcessingWhenUsingD3DAndAdvancedProcessing) {
  EXPECT_TRUE(!wallpaper::ShouldUseLegacySourceReaderVideoProcessing(true, true));
}

TEST_CASE(DecodeOutputPolicy_EnablesLegacyVideoProcessingWhenNotInD3DAdvancedPath) {
  EXPECT_TRUE(wallpaper::ShouldUseLegacySourceReaderVideoProcessing(false, true));
  EXPECT_TRUE(wallpaper::ShouldUseLegacySourceReaderVideoProcessing(true, false));
}
