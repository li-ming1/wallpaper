#include "wallpaper/tray_action_queue.h"

#include "test_support.h"

TEST_CASE(TrayActionQueue_PreservesFifoOrderWithinCapacity) {
  wallpaper::TrayActionQueue queue(3);
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kMenuOpened, {}}));
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kSelectVideo, "a"}));
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kMenuClosed, {}}));

  wallpaper::TrayAction action;
  EXPECT_TRUE(queue.Dequeue(&action));
  EXPECT_EQ(static_cast<int>(action.type),
            static_cast<int>(wallpaper::TrayActionType::kMenuOpened));
  EXPECT_TRUE(queue.Dequeue(&action));
  EXPECT_EQ(static_cast<int>(action.type),
            static_cast<int>(wallpaper::TrayActionType::kSelectVideo));
  EXPECT_EQ(action.payload, "a");
  EXPECT_TRUE(queue.Dequeue(&action));
  EXPECT_EQ(static_cast<int>(action.type),
            static_cast<int>(wallpaper::TrayActionType::kMenuClosed));
}

TEST_CASE(TrayActionQueue_DropsOldestWhenFull) {
  wallpaper::TrayActionQueue queue(2);
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kMenuOpened, {}}));
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kMenuClosed, {}}));
  EXPECT_TRUE(queue.Enqueue(wallpaper::TrayAction{wallpaper::TrayActionType::kExit, {}}));

  wallpaper::TrayAction action;
  EXPECT_TRUE(queue.Dequeue(&action));
  EXPECT_EQ(static_cast<int>(action.type),
            static_cast<int>(wallpaper::TrayActionType::kMenuClosed));
  EXPECT_TRUE(queue.Dequeue(&action));
  EXPECT_EQ(static_cast<int>(action.type), static_cast<int>(wallpaper::TrayActionType::kExit));
}
