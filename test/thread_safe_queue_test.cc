#include <thread>

#include "base/locked_queue.h"
#include "base/simple_locked_queue.h"
#include "gtest/gtest.h"

void OneProduceOneConsumer(std::shared_ptr<ThreadSafeQueue<int>> q) {
  std::thread produce(
      [](ThreadSafeQueue<int>& q) {
        for (int i = 0; i < 20; i++) {
          q.Push(i);
        }
      },
      std::ref(*q));

  std::thread consumer(
      [](ThreadSafeQueue<int>& q) {
        for (int i = 0; i < 20; i++) {
          EXPECT_EQ(*q.WaitAndPop(), i);
        }
      },
      std::ref(*q));

  produce.join();
  consumer.join();
}

TEST(ThreadSafeQueueTest, SimpleLockedQueueTest) {
  auto q = std::make_shared<SimpleLockedQueue<int>>();

  OneProduceOneConsumer(q);
}

TEST(ThreadSafeQueueTest, LockedQueueTest) {
  auto q = std::make_shared<LockedQueue<int>>();

  OneProduceOneConsumer(q);
}