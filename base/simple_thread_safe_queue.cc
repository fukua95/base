#include "base/simple_thread_safe_queue.h"

template <typename T>
bool ThreadSafeQueue<T>::Empty() const {
  std::lock_guard lg(mu_);
  return queue_.empty();
}

template <typename T>
void ThreadSafeQueue<T>::Push(T value) {
  auto data = std::make_shared<T>(std::move(value));
  std::lock_guard lg(mu_);
  queue_.push(data);
  cv_.notify_one();
}

template <typename T>
void ThreadSafeQueue<T>::WaitAndPop(T* value) {
  std::unique_lock ul(mu_);
  cv_.wait(ul, [this] { return !queue_.empty(); });
  *value = std::move(*queue_.front());
  queue_.pop();
}

template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::WaitAndPop() {
  std::unique_lock ul(mu_);
  cv_.wait(ul, [this] { return !queue_.empty(); });
  auto res = queue_.front();
  queue_.pop();
  return res;
}

template <typename T>
bool ThreadSafeQueue<T>::TryPop(T* value) {
  std::lock_guard lg(mu_);
  if (queue_.empty()) {
    return false;
  }
  *value = std::move(*queue_.front());
  queue_.pop();
  return true;
}

template <typename T>
std::shared_ptr<T> ThreadSafeQueue<T>::TryPop() {
  std::lock_guard lg(mu_);
  if (queue_.empty()) {
    return std::shared_ptr<T>();
  }
  auto res = queue_.front();
  queue_.pop();
  return res;
}

// 对于模板类，还是不做定义和实现分离，一起放在 .h 中更加方便
// 如果实现分离到 .cc 中，需要显式实例化
template class ThreadSafeQueue<int>;
