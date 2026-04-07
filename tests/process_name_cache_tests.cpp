#include "wallpaper/process_name_cache.h"

#include <cstdint>
#include <string>

#include "test_support.h"

TEST_CASE(ProcessNameCache_HitsCachedSuccessfulLookup) {
  wallpaper::ProcessNameCache cache;
  cache.RememberSuccess(
      100U, std::wstring{L'e', L'x', L'p', L'l', L'o', L'r', L'e', L'r', L'.', L'e', L'x', L'e'});

  std::wstring name;
  const auto lookup = cache.TryGet(100U, &name);

  EXPECT_EQ(static_cast<int>(lookup),
            static_cast<int>(wallpaper::ProcessNameCacheLookup::kHit));
  EXPECT_TRUE(name == L"explorer.exe");
}

TEST_CASE(ProcessNameCache_HitsCachedFailureWithoutName) {
  wallpaper::ProcessNameCache cache;
  cache.RememberFailure(200U);

  std::wstring name = L"stale";
  const auto lookup = cache.TryGet(200U, &name);

  EXPECT_EQ(static_cast<int>(lookup),
            static_cast<int>(wallpaper::ProcessNameCacheLookup::kKnownFailure));
  EXPECT_TRUE(name.empty());
}

TEST_CASE(ProcessNameCache_MissesUnknownProcessId) {
  wallpaper::ProcessNameCache cache;
  std::wstring name = L"unchanged";

  const auto lookup = cache.TryGet(300U, &name);

  EXPECT_EQ(static_cast<int>(lookup),
            static_cast<int>(wallpaper::ProcessNameCacheLookup::kMiss));
  EXPECT_TRUE(name == L"unchanged");
}

TEST_CASE(ProcessNameCache_EvictsOldestEntryWhenCapacityExceeded) {
  wallpaper::ProcessNameCache cache;
  for (std::uint32_t processId = 1; processId <= cache.Capacity(); ++processId) {
    cache.RememberSuccess(processId, std::wstring{L'c', L'a', L'c', L'h', L'e', L'd'});
  }
  cache.RememberSuccess(cache.Capacity() + 1U,
                        std::wstring{L'n', L'e', L'w', L'e', L's', L't'});

  std::wstring name;
  EXPECT_EQ(static_cast<int>(cache.TryGet(1U, &name)),
            static_cast<int>(wallpaper::ProcessNameCacheLookup::kMiss));
  EXPECT_EQ(static_cast<int>(cache.TryGet(cache.Capacity() + 1U, &name)),
            static_cast<int>(wallpaper::ProcessNameCacheLookup::kHit));
  EXPECT_TRUE(name == L"newest");
}
