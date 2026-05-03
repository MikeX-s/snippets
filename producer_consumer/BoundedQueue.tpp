#pragma once

template <typename T>
BoundedQueue<T>::BoundedQueue(std::size_t capacity, DropPolicy policy)
    : _capacity{capacity}, _policy{policy} {}

template <typename T>
template <typename U>
std::expected<void, T> BoundedQueue<T>::push(
    U&& item,
    std::stop_token stop,
    std::chrono::duration<double> timeout) {
  std::unique_lock<std::mutex> lock{_queueMtx};

  if (_policy == DropPolicy::Block) {
    auto predicate = [this] { return _queue.size() < _capacity; };
    bool success = false;

    if (timeout == std::chrono::duration<double>::max()) {
      success = _cv.wait(lock, stop, std::move(predicate));
    } else {
      success = _cv.wait_for(lock, stop, timeout, std::move(predicate));
    }

    if (!success) {
      return std::unexpected(std::move(item));
    }
  } else if (_queue.size() >= _capacity) {
    if (_policy == DropPolicy::DropNewest)
      return std::unexpected(std::forward<U>(item));
    if (_policy == DropPolicy::DropOldest)
      _queue.pop();
  }

  if (stop.stop_requested())
    return std::unexpected(std::forward<U>(item));

  _queue.push(std::forward<U>(item));
  _cv.notify_one();

  return {};
}

template <typename T>
bool BoundedQueue<T>::pop(T& out, std::stop_token stop) {
  std::unique_lock<std::mutex> lock{_queueMtx};

  _cv.wait(lock, stop, [this] { return !_queue.empty(); });

  if (_queue.empty()) {
    return false;
  }
  out = std::move(_queue.front());
  _queue.pop();
  _cv.notify_all();

  return true;
}

template <typename T>
void BoundedQueue<T>::waitUntilEmpty(std::stop_token stop) {
  std::unique_lock lock{_queueMtx};
  _cv.wait(lock, stop, [this] { return _queue.empty(); });
}

template <typename T>
std::size_t BoundedQueue<T>::size() const {
  std::unique_lock<std::mutex> lock{_queueMtx};

  return _queue.size();
}

template <typename T>
bool BoundedQueue<T>::empty() const {
  std::unique_lock<std::mutex> lock{_queueMtx};

  return _queue.empty();
}