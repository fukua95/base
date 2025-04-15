#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>

template <typename T>
class LockedQueue {
 public:
  LockedQueue() : head_(new Node), tail_(head_.get()) {}
  LockedQueue(const LockedQueue& other) = delete;
  LockedQueue& operator=(const LockedQueue& other) = delete;

  bool Empty() {
    std::lock_guard head_lock(head_mu_);
    return head_.get() == GetTail();
  }

  std::shared_ptr<T> TryPop() {
    std::unique_ptr<Node> old_head = TryPopHead();
    return old_head ? old_head->data_ : std::shared_ptr<T>();
  }

  bool TryPop(T* value) {
    std::unique_ptr<Node> old_head = TryPopHead(value);
    return old_head != nullptr;
  }

  std::shared_ptr<T> WaitAndPop() {
    auto old_head = WaitPopHead();
    return old_head->data_;
  }

  void WaitAndPop(T* value) { WaitPopHead(value); }

  void Push(T value) {
    auto data = std::make_shared<T>(std::move(value));
    std::unique_ptr<Node> p(new Node);

    {
      std::lock_guard tail_lg(tail_mu_);
      tail_->data_ = data;
      auto new_tail = p.get();
      tail_->next_ = std::move(p);
      tail_ = new_tail;
    }
    cv_.notify_one();
  }

 private:
  struct Node {
    std::shared_ptr<T> data_;
    std::unique_ptr<Node> next_;
  };

  Node* GetTail() {
    std::lock_guard tail_lg(tail_mu_);
    return tail_;
  }

  std::unique_ptr<Node> PopHead() {
    std::unique_ptr<Node> old_head = std::move(head_);
    head_ = std::move(old_head->next_);
    return old_head;
  }

  std::unique_lock<std::mutex> WaitForData() {
    std::unique_lock head_lock(head_mu_);
    cv_.wait(head_lock, [&] { return head_.get() != GetTail(); });
    // return std::move(head_lock);
    return head_lock;
  }

  std::unique_ptr<Node> WaitPopHead() {
    std::unique_lock head_lock(WaitForData());
    return PopHead();
  }

  std::unique_ptr<Node> WaitPopHead(T* value) {
    std::unique_lock head_lock(WaitForData());
    *value = std::move(*head_->data_);
    return PopHead();
  }

  std::unique_ptr<Node> TryPopHead() {
    std::lock_guard head_lock(head_mu_);
    if (head_.get() == GetTail()) {
      return std::unique_ptr<Node>();
    }
    return PopHead();
  }

  std::unique_ptr<Node> TryPopHead(T* value) {
    std::lock_guard head_lock(head_mu_);
    if (head_.get() == GetTail()) {
      return std::unique_ptr<Node>();
    }
    *value = std::move(*head_->data_);
    return PopHead();
  }

  std::mutex head_mu_;
  std::unique_ptr<Node> head_;
  std::mutex tail_mu_;
  Node* tail_;
  // 当只有一个节点时，head_ 和 tail_ 指向同一个节点，head_ 是 unique_ptr, 所以
  // tail 只能是裸指针，或者都为 shared_ptr.
  std::condition_variable cv_;
};

// clangd friendly when developing.
// template class LockedQueue<int>;