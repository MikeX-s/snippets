#pragma once

#include <concepts>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
concept SequenceContainerWithBack =
    requires(T container, typename T::value_type value) {
      { container.back() } -> std::same_as<typename T::reference>;
      { container.push_back(value) } -> std::same_as<void>;
      typename T::allocator_type;
    } && !std::is_base_of_v<std::basic_string<typename T::value_type>, T>;

template <typename T, SequenceContainerWithBack ContainerT = std::deque<T>>
class ThreadSafeStack {
 public:
  using MutexT = std::shared_mutex;

  ThreadSafeStack() = default;
  ~ThreadSafeStack() = default;

  ThreadSafeStack(const ThreadSafeStack& other) : _data{other._data} {}
  ThreadSafeStack& operator=(const ThreadSafeStack& other) {
    _data = other._data;
    return *this;
  }
  ThreadSafeStack(ThreadSafeStack&& other) noexcept : _data{std::exchange(other._data, {})} {}
  ThreadSafeStack& operator=(ThreadSafeStack&& other) noexcept {
    _data = std::exchange(other._data, {});
    return *this;
  }

  const T& top() {
    std::shared_lock<MutexT> lock{_mtx};
    if (_data.empty()) {
      throw std::out_of_range("top() called on empty ThreadSafeStack");
    }

    return _data.back();
  }

  void push(auto&& item) {
    std::unique_lock<MutexT> lock{_mtx};
    _data.push_back(std::forward<T>(item));
  }

  std::shared_ptr<T> pop() {
    std::unique_lock<MutexT> lock{_mtx};
    if (_data.empty()) {
      return nullptr;
    }

    auto res = std::make_shared<T>(std::move(_data.back()));
    _data.pop_back();

    return res;
  }

  bool empty() const {
    std::shared_lock<MutexT> lock{_mtx};
    return _data.empty();
  }

  std::size_t size() const {
    std::shared_lock<MutexT> lock{_mtx};
    return _data.size();
  }

 private:
  mutable MutexT _mtx;
  ContainerT _data{};
};