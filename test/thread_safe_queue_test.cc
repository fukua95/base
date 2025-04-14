#include <thread>

#include "base/simple_thread_safe_queue.h"
#include "gtest/gtest.h"

TEST(ThreadSafeQueueTest, OneProduceOneConsumer) {
  ThreadSafeQueue<int> q;
  std::thread produce(
      [](ThreadSafeQueue<int>& q) {
        for (int i = 0; i < 5; i++) {
          q.Push(i);
        }
      },
      std::ref(q));

  std::thread consumer(
      [](ThreadSafeQueue<int>& q) {
        for (int i = 0; i < 5; i++) {
          EXPECT_EQ(*q.WaitAndPop(), i);
        }
      },
      std::ref(q));

  produce.join();
  consumer.join();
}