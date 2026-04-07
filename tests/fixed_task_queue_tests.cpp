#include "wallpaper/fixed_task_queue.h"

#include <string>

#include "test_support.h"

TEST_CASE(FixedTaskQueue_PushBackDropsOldestWhenFull) {
  wallpaper::FixedTaskQueue<int> queue(2);
  EXPECT_TRUE(queue.PushBack(1));
  EXPECT_TRUE(queue.PushBack(2));
  EXPECT_TRUE(queue.PushBack(3));

  int value = 0;
  EXPECT_TRUE(queue.PopFront(&value));
  EXPECT_EQ(value, 2);
  EXPECT_TRUE(queue.PopFront(&value));
  EXPECT_EQ(value, 3);
}

TEST_CASE(FixedTaskQueue_PeekFrontTracksWraparoundOrder) {
  wallpaper::FixedTaskQueue<std::string> queue(2);
  EXPECT_TRUE(queue.PushBack(std::string("a")));
  EXPECT_TRUE(queue.PushBack(std::string("b")));

  std::string value;
  EXPECT_TRUE(queue.PopFront(&value));
  EXPECT_EQ(value, "a");
  EXPECT_TRUE(queue.PushBack(std::string("c")));
  EXPECT_EQ(*queue.Front(), "b");
}
