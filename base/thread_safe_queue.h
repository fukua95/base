#pragma once

#include <memory>

template <typename T>
class ThreadSafeQueue {
 public:
  virtual ~ThreadSafeQueue() = default;

  virtual bool Empty() const = 0;

  virtual void Push(T value) = 0;

  virtual void WaitAndPop(T* value) = 0;

  virtual std::shared_ptr<T> WaitAndPop() = 0;

  virtual bool TryPop(T* value) = 0;

  virtual std::shared_ptr<T> TryPop() = 0;
};