#include "wallpaper/desktop_context_policy.h"

#include "test_support.h"

TEST_CASE(DesktopContextPolicy_ShellAndExplorerClassesAreDesktopContext) {
  EXPECT_TRUE(wallpaper::IsDesktopContextClass(L"Progman"));
  EXPECT_TRUE(wallpaper::IsDesktopContextClass(L"WorkerW"));
  EXPECT_TRUE(wallpaper::IsDesktopContextClass(L"CabinetWClass"));
}

TEST_CASE(DesktopContextPolicy_ExplorerProcessIsDesktopContext) {
  EXPECT_TRUE(wallpaper::ShouldTreatAsDesktopContext(L"RandomClass", L"explorer.exe"));
  EXPECT_TRUE(wallpaper::ShouldTreatAsDesktopContext(L"RandomClass", L"EXPLORER.EXE"));
}

TEST_CASE(DesktopContextPolicy_NonDesktopAppIsNotDesktopContext) {
  EXPECT_TRUE(!wallpaper::ShouldTreatAsDesktopContext(L"ApplicationFrameWindow", L"taskmgr.exe"));
}
