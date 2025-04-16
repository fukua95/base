#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "base/thread_safe_queue.h"

// `SimpleLockedQueue1` 实现有一个问题，场景：
// 多个线程调用 WaitAndPop() 等待数据，一个新线程调用 Push() -> notify_one(),
// 一个等待线程会被唤醒，继续执行到 std::make_shared，
// std::make_shared 时没有足够的内存，会抛出 std::bad_alloc 异常，
// 此时线程直接返回，导致 `queue_` 有数据，但是其他等待线程没有被唤醒.
// 解决方案：
// 1. notify_one() -> notify_all()，正常场景下很浪费资源
// 2. WaitAndPop() 中捕获 std::make_shared 的异常，有异常时再调用一次
// notify_one().
// 3. std::queue<T> 改为 std::queue<std::shared_ptr<T>>，避免 std::make_shared.
//    `SimpleLockedQueue` 采用方案3.
template <typename T>
class SimpleLockedQueue1 : public ThreadSafeQueue<T> {
 public:
  SimpleLockedQueue1() {}

  bool Empty() const override {
    std::lock_guard lg(mu_);
    return queue_.empty();
  }

  void Push(T value) override {
    std::lock_guard lg(mu_);
    queue_.push(std::move(value));
    cv_.notify_one();
  }

  void WaitAndPop(T* value) override {
    std::unique_lock ul(mu_);
    cv_.wait(ul, [this] { return !queue_.empty(); });
    *value = std::move(queue_.front());
    queue_.pop();
  }

  std::shared_ptr<T> WaitAndPop() override {
    std::unique_lock ul(mu_);
    cv_.wait(ul, [this] { return !queue_.empty(); });
    auto value = std::make_shared<T>(std::move(queue_.front()));
    queue_.pop();
    return value;
  }

  bool TryPop(T* value) override {
    std::lock_guard lg(mu_);
    if (queue_.empty()) {
      return false;
    }
    *value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  std::shared_ptr<T> TryPop() override {
    std::lock_guard lg(mu_);
    if (queue_.empty()) {
      return std::shared_ptr<T>();
    }
    auto res = std::make_shared<T>(std::move(queue_.front()));
    queue_.pop();
    return res;
  }

 private:
  mutable std::mutex mu_;
  std::queue<T> queue_;
  std::condition_variable cv_;
};

template <typename T>
class SimpleLockedQueue : public ThreadSafeQueue<T> {
 public:
  SimpleLockedQueue() {}

  bool Empty() const override;

  void Push(T value) override;

  void WaitAndPop(T* value) override;

  std::shared_ptr<T> WaitAndPop() override;

  bool TryPop(T* value) override;

  std::shared_ptr<T> TryPop() override;

 private:
  mutable std::mutex mu_;
  std::queue<std::shared_ptr<T>> queue_;
  std::condition_variable cv_;
};
