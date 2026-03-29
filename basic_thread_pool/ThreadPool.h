#pragma once

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>
#include <vector>

// Pure interface for a type-erased invocable.
class CallableBase {
 public:
  virtual ~CallableBase() = default;
  virtual void operator()() = 0;
};

template <typename F>
class CallableImpl final : public CallableBase {
 public:
  explicit CallableImpl(F&& fun) : m_fun(std::move(fun)) {}
  void operator()() override { std::invoke(m_fun); }

 private:
  F m_fun;
};

class Task {
 public:
  Task() = default;
  ~Task() = default;

  template <typename F>
  Task(F&& fun)  // Intentionally implicit: Task t = [](){};
      : m_callable(std::make_unique<CallableImpl<F>>(std::forward<F>(fun))) {}

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) noexcept = default;
  Task& operator=(Task&&) noexcept = default;

  void operator()() const {
    if (!m_callable)
      throw std::bad_function_call{};

    std::invoke(*m_callable);
  }

 private:
  std::unique_ptr<CallableBase> m_callable = nullptr;
};

class ThreadPool {
 public:
  explicit ThreadPool(unsigned int thread_count =
                          std::max(4u, std::thread::hardware_concurrency()));
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  auto submit(std::invocable auto&& fun) {
    using F = decltype(fun);
    using R = std::invoke_result_t<F>;

    std::packaged_task<R()> packaged(std::forward<F>(fun));
    std::future<R> future = packaged.get_future();

    std::lock_guard lock(m_mutex);
    m_queue.push(std::move(packaged));

    return future;
  }

 private:
  void worker();
  bool pop_task(Task& task);

  std::atomic<bool> m_running = true;
  std::mutex m_mutex;
  std::queue<Task> m_queue;
  std::vector<std::thread> m_threads;
};