#include "wallpaper/video_path_matcher.h"

#include "test_support.h"

#include <filesystem>
#include <string>

TEST_CASE(VideoPathMatcher_EmptyPathsAreSame) {
  EXPECT_TRUE(wallpaper::IsSameVideoPath({}, {}));
  EXPECT_TRUE(!wallpaper::IsSameVideoPath({}, "a.mp4"));
}

TEST_CASE(VideoPathMatcher_RelativeAndAbsolutePathAreSame) {
  const std::filesystem::path relative = std::filesystem::path("videos") / "demo.mp4";
  const std::filesystem::path absolute = std::filesystem::absolute(relative).lexically_normal();
  EXPECT_TRUE(wallpaper::IsSameVideoPath(relative.generic_string(), absolute.generic_string()));
}

TEST_CASE(VideoPathMatcher_DifferentPathIsNotSame) {
  EXPECT_TRUE(!wallpaper::IsSameVideoPath("videos/a.mp4", "videos/b.mp4"));
}

#ifdef _WIN32
TEST_CASE(VideoPathMatcher_WindowsCaseAndSeparatorInsensitive) {
  EXPECT_TRUE(
      wallpaper::IsSameVideoPath("C:\\Users\\Public\\Videos\\Demo.MP4",
                                 "c:/users/public/videos/./demo.mp4"));
}
#endif
