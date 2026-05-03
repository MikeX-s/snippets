#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

enum class DropPolicy {
  DropNewest,  // publisher never blocks
  DropOldest,  // always holds most recent N events
  Block        // publisher waits until a slot is free
};

template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity,
                        DropPolicy policy = DropPolicy::Block);

  template <typename U>
  std::expected<void, T> push(U&& item, std::stop_token stop);
  bool pop(T& out, std::stop_token stop);

  std::size_t size() const;
  bool empty() const;

 private:
  const DropPolicy _policy;
  const std::size_t _capacity;
  mutable std::mutex _queueMtx;
  std::condition_variable_any _cv;
  std::queue<T> _queue{};
};

#include "BoundedQueue.tpp"