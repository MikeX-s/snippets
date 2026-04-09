#pragma once

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <atomic>
#include <memory>
#include <utility>

template <typename T>
class Node {
 public:
  explicit Node(T&& value)
      : data{std::make_shared<T>(std::forward<T>(value))} {}

  std::shared_ptr<T> data{};
  std::shared_ptr<Node<T>> next = nullptr;
};

template <typename T>
class LockFreeStack {
 public:
  LockFreeStack() = default;
  ~LockFreeStack() {
    while (pop()) {
    }
  }

  LockFreeStack(const LockFreeStack& other) : count{other.count.load()} {
    auto old_head =
        this->head.exchange(std::make_shared<Node<T>>(*other.head.load()));
    destroy(old_head);
  }

  LockFreeStack& operator=(const LockFreeStack& other) {
    this->count = other.count.load();
    auto old_head =
        this->head.exchange(std::make_shared<Node<T>>(*other.head.load()));
    destroy(old_head);
    return *this;
  }

  LockFreeStack(LockFreeStack&& other) noexcept {
    this->count.exchange(other.count);
    auto old_head = this->head.exchange(other.head);
    destroy(old_head);
  }

  LockFreeStack& operator=(LockFreeStack&& other) noexcept {
    this->count.exchange(other.count);
    auto old_head = this->head.exchange(other.head);
    destroy(old_head);
    return *this;
  }

  void push(auto&& data) {
    auto new_node = std::make_shared<Node<T>>(std::forward<T>(data));

    // put the current value of head into new_node->next
    new_node->next = head.load(std::memory_order_relaxed);

    // now make new_node the new head, but if the head
    // is no longer what's stored in new_node->next
    // then put that new head into new_node->next and try again
    while (!head.compare_exchange_weak(new_node->next, new_node,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
    }

    count.fetch_add(1);
  }

  std::shared_ptr<T> pop() {
    auto current_head = head.load(std::memory_order_relaxed);

    while ((current_head != nullptr) and
           !head.compare_exchange_weak(
               current_head, std::atomic_load(&current_head->next),
               std::memory_order_acquire, std::memory_order_relaxed)) {
    }

    if (current_head) {
      std::atomic_store(&current_head->next, std::shared_ptr<Node<T>>());
      count.fetch_sub(1);

      return current_head->data;
    }

    return nullptr;
  }

  const T& top() {
    auto ptr = head.load(std::memory_order_acquire);
    if (ptr == nullptr) {
      throw std::out_of_range("top() called on empty stack");
    }

    return *ptr->data;
  }

  std::size_t size() { return count.load(std::memory_order_relaxed); }

  bool empty() { return head.load(std::memory_order_relaxed) == nullptr; }

 private:
  static void destroy(std::shared_ptr<Node<T>> old_head) {
    while (old_head) {
      auto next = old_head->next;
      old_head = next;
    }
  }

  std::atomic<int> count = 0;
  std::atomic<std::shared_ptr<Node<T>>> head = nullptr;
};