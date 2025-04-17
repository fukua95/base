#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "base/locked_queue.h"

class JoinThreads {
 public:
  explicit JoinThreads(std::vector<std::thread>& threads) : threads_(threads) {}
  ~JoinThreads() {
    for (size_t i = 0; i < threads_.size(); i++) {
      if (threads_[i].joinable()) {
        threads_[i].join();
      }
    }
  }

 private:
  std::vector<std::thread>& threads_;
};

class FunctionWrapper {
 public:
  FunctionWrapper() = default;
  template <typename F>
  FunctionWrapper(F&& f) : impl_(std::make_unique<ImplType>(std::move(f))) {}
  FunctionWrapper(FunctionWrapper&& other) : impl_(std::move(other.impl_)) {}
  FunctionWrapper& operator=(FunctionWrapper&& other) {
    impl_ = std::move(other.impl_);
    return *this;
  }
  FunctionWrapper(const FunctionWrapper&) = delete;
  FunctionWrapper& operator=(const FunctionWrapper&) = delete;

  void operator()() { impl_->Call(); }

 private:
  struct ImplBase {
    virtual void Call() = 0;
    virtual ~ImplBase() {}
  };

  template <typename F>
  struct ImplType : public ImplBase {
    F f_;
    ImplType(F&& f) : f_(std::move(f)) {}
    void Call() override { f_(); }
  };

  std::unique_ptr<ImplBase> impl_;
};

// Push() -> push_front(), TryPop() -> pop_front():
// `WorkStealingQueue` is a last-in-first-out stack, this can help improve
// performance from a cache perspective.
// TrySteal() -> pop_back():
// in order to minimize contention.
class WorkStealingQueue {
 public:
  WorkStealingQueue() {}
  WorkStealingQueue(const WorkStealingQueue& other) = delete;
  WorkStealingQueue& operator=(const WorkStealingQueue& other) = delete;

  void Push(FunctionWrapper data) {
    std::lock_guard lg(mu_);
    q_.push_front(std::move(data));
  }

  bool TryPop(FunctionWrapper* value) {
    std::lock_guard lg(mu_);
    if (q_.empty()) {
      return false;
    }
    *value = std::move(q_.front());
    q_.pop_front();
    return true;
  }

  bool TrySteal(FunctionWrapper* value) {
    std::lock_guard lg(mu_);
    if (q_.empty()) {
      return false;
    }
    *value = std::move(q_.back());
    q_.pop_back();
    return true;
  }

  bool Empty() const {
    std::lock_guard lg(mu_);
    return q_.empty();
  }

 private:
  using data_type = FunctionWrapper;
  mutable std::mutex mu_;
  std::deque<data_type> q_;
};

class ThreadPool {
 public:
  ThreadPool() : done_(false), joiner_(threads_) {
    auto thread_count = std::thread::hardware_concurrency();
    try {
      for (size_t i = 0; i < thread_count; i++) {
        local_task_queues_.push_back(std::make_unique<WorkStealingQueue>());
      }

      for (size_t i = 0; i < thread_count; i++) {
        threads_.push_back(std::thread(&ThreadPool::Worker, this, i));
      }
    } catch (...) {
      done_ = true;
      throw;
    }
  }

  ~ThreadPool() { done_ = true; }

  template <typename F>
  std::future<typename std::invoke_result_t<F()>> Submit(F f) {
    using result_t = std::invoke_result_t<F()>;
    std::packaged_task<result_t()> task(std::move(f));
    std::future<result_t> res = task.get_future();

    if (local_task_queue_) {
      local_task_queue_->Push(std::move(task));
    } else {
      global_task_queue_.Push(std::move(task));
    }

    return res;
  }

  void RunPendingTask() {
    FunctionWrapper task;
    if (PopTaskFromLocalQueue(&task) || PopTaskFromGlobalQueue(&task) ||
        PopTaskFromOtherThreadQueue(&task)) {
      task();
    } else {
      std::this_thread::yield();
    }
  }

 private:
  using task_t = FunctionWrapper;

  void Worker(unsigned index) {
    my_index_ = index;
    local_task_queue_ = local_task_queues_[index].get();
    while (!done_) {
      RunPendingTask();
    }
  }

  bool PopTaskFromLocalQueue(task_t* task) {
    return local_task_queue_ && local_task_queue_->TryPop(task);
  }

  bool PopTaskFromGlobalQueue(task_t* task) {
    return global_task_queue_.TryPop(task);
  }

  bool PopTaskFromOtherThreadQueue(task_t* task) {
    for (unsigned i = 0; i < local_task_queues_.size(); i++) {
      auto index = (my_index_ + 1 + i) % local_task_queues_.size();
      if (local_task_queues_[index]->TrySteal(task)) {
        return true;
      }
    }
    return false;
  }

  // 定义的顺序：
  // threads_ 必须在 joiner_ 前,
  // done_ & task queue 必须在 threads_ 前，因为线程执行(Worker)会访问它们.

  std::atomic_bool done_;
  LockedQueue<task_t> global_task_queue_;
  std::vector<std::unique_ptr<WorkStealingQueue>> local_task_queues_;
  // `local_task_queue_` point to `local_task_queues_[my_index_]`
  static thread_local WorkStealingQueue* local_task_queue_;
  static thread_local unsigned my_index_;
  std::vector<std::thread> threads_;
  JoinThreads joiner_;
};