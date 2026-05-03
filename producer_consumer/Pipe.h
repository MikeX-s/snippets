#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <functional>
#include <new>
#include <queue>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include "BoundedQueue.h"
#include "Utils.h"

enum class PipePolicy {
  Block,  //
  DropOldest,
  DropNewest
};

enum class PipeState {
  Created,  //
  Running,
  Closed
};

enum class PipeError {
  Timeout,  //
  Closed,
  Dropped,
  Full
};

constexpr DropPolicy toDropPolicy(PipePolicy p) noexcept {
  switch (p) {
    case PipePolicy::Block:
      return DropPolicy::Block;
    case PipePolicy::DropNewest:
      return DropPolicy::DropNewest;
    case PipePolicy::DropOldest:
      return DropPolicy::DropOldest;
    default:
      std::unreachable();
  }
}

struct PipeStats {
 private:
  static constexpr size_t CacheAlign =
      std::hardware_destructive_interference_size;

 public:
  alignas(CacheAlign) std::atomic<std::size_t> pushed{};
  alignas(CacheAlign) std::atomic<std::size_t> dropped{};
  alignas(CacheAlign) std::atomic<std::size_t> consumed{};
  alignas(CacheAlign) std::atomic<std::size_t> produced{};
  alignas(CacheAlign) std::atomic<std::size_t> popped{};
  alignas(CacheAlign) std::atomic<std::size_t> queued{};

  void reset() noexcept {
    pushed.store(0, std::memory_order_relaxed);
    dropped.store(0, std::memory_order_relaxed);
    consumed.store(0, std::memory_order_relaxed);
    produced.store(0, std::memory_order_relaxed);
    popped.store(0, std::memory_order_relaxed);
    queued.store(0, std::memory_order_relaxed);
  }
};

template <typename Fn, typename T>
concept ProducerCallable =
    std::invocable<Fn> &&
    std::convertible_to<std::invoke_result_t<Fn>, std::expected<T, PipeError>>;

template <typename Fn, typename T>
concept ConsumerCallable = std::invocable<Fn, T> &&
                           std::convertible_to<std::invoke_result_t<Fn, T>,
                                               std::expected<void, PipeError>>;

class PipeHandle {
 public:
  explicit PipeHandle(std::jthread thread) : _thread{std::move(thread)} {}
  ~PipeHandle() = default;

  PipeHandle(const PipeHandle&) = delete;
  PipeHandle& operator=(const PipeHandle&) = delete;

  PipeHandle(PipeHandle&& other) noexcept = default;
  PipeHandle& operator=(PipeHandle&& other) noexcept = default;

  void stop() {
    if (_thread.joinable()) {
      _thread.request_stop();
    }
  }

 private:
  std::jthread _thread;
};

template <typename T>
class Pipe {
 public:
  using Duration = std::chrono::duration<double>;

  explicit Pipe(std::size_t capacity,
                PipePolicy policy = PipePolicy::Block,
                Duration timeout = Duration::max())
      : _queue{capacity, toDropPolicy(policy)},
        _policy{policy},
        _timeout{timeout} {}
  ~Pipe() { close(); }

  // non-copyable, non-movable
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  Pipe(Pipe&&) noexcept = delete;
  Pipe& operator=(Pipe&&) noexcept = delete;

  void start() {
    std::call_once(_init_flag, [this] {
      _state.store(PipeState::Running, std::memory_order_release);
    });
  }

  const PipeStats& getStats() const { return _monitor.stats(); }

  [[nodiscard]] bool setCbOnDrop(std::function<void(T)> cb) {
    if (isRunning())
      return false;
    _monitor.setDeadLetter(std::move(cb));
    return true;
  }

  bool isRunning() const noexcept {
    return _state.load(std::memory_order_acquire) == PipeState::Running;
  }
  bool isClosed() const noexcept {
    return _state.load(std::memory_order_acquire) == PipeState::Closed;
  }

  void close() {
    if (_state.exchange(PipeState::Closed, std::memory_order_acq_rel) ==
        PipeState::Closed) {
      return;
    }
    _stopSource.request_stop();

    _ownedProducers.clear();

    _queue.waitUntilEmpty(_stopSource.get_token());

    _ownedConsumers.clear();
  }

  template <typename Fn>
    requires ProducerCallable<Fn, T>
  [[nodiscard]] PipeHandle produce(Fn&& fn) {
    assert(!isClosed());
    start();

    auto pushOp = [this](T&& item) -> std::expected<void, T> {
      return _queue.push(std::move(item), _stopSource.get_token(), _timeout);
    };
    auto task = _monitor.wrapProducer(std::forward<Fn>(fn), std::move(pushOp));

    return PipeHandle{
        std::jthread{[this, task = std::move(task)](std::stop_token stop) {
          while (!stop.stop_requested() and !_stopSource.stop_requested()) {
            try {
              std::invoke(task);
            } catch (const std::exception& e) {
              Log::logError(e.what());
              break;
            }
          }
        }}};
  }

  template <typename Fn>
    requires ConsumerCallable<Fn, T>
  [[nodiscard]] PipeHandle consume(Fn&& fn) {
    assert(!isClosed());
    start();

    auto popOp = [this](T& out, std::stop_token stop) {
      return _queue.pop(out, stop);
    };
    auto task = _monitor.wrapConsumer(std::forward<Fn>(fn), std::move(popOp));

    return PipeHandle{
        std::jthread{[this, task = std::move(task)](std::stop_token stop) {
          T item{};

          while (!stop.stop_requested() and !_stopSource.stop_requested()) {
            try {
              if (!task(item, stop))
                break;
            } catch (const std::exception& e) {
              Log::logError(e.what());
              break;
            }
          }
        }}};
  }

  template <typename Fn>
    requires ProducerCallable<Fn, T>
  void produceMany(std::size_t n, Fn&& fn) {
    assert(!isClosed());

    for (auto _ : std::views::repeat(0, n)) {
      _ownedProducers.push_back(produce(fn));
    }
  }

  template <typename Fn>
    requires ConsumerCallable<Fn, T>
  void consumeMany(std::size_t n, Fn&& fn) {
    assert(!isClosed());

    for (auto _ : std::views::repeat(0, n)) {
      _ownedConsumers.push_back(consume(fn));
    }
  }

 private:
  class PipeMonitor {
   public:
    PipeMonitor() = default;
    ~PipeMonitor() = default;
    PipeMonitor(const PipeMonitor&) = delete;
    PipeMonitor& operator=(const PipeMonitor&) = delete;
    PipeMonitor(PipeMonitor&&) noexcept = delete;
    PipeMonitor& operator=(PipeMonitor&&) noexcept = delete;

    const PipeStats& stats() const noexcept { return _stats; }
    void setDeadLetter(std::function<void(T)> cb) {
      _deadLetter = std::move(cb);
    }

    auto wrapProducer(auto&& fn, auto&& pushOp) {
      return [fn = std::forward<decltype(fn)>(fn),
              pushOp = std::forward<decltype(pushOp)>(pushOp), &stats = _stats,
              &cb = _deadLetter]() {
        auto result = std::invoke(fn);
        ++stats.produced;

        if (result.has_value()) {
          auto pushResult = std::invoke(pushOp, std::move(result.value()));

          if (pushResult.has_value()) {
            ++stats.pushed;
            ++stats.queued;
          } else {
            ++stats.dropped;
            if (cb)
              std::invoke(*cb, std::move(pushResult.error()));
          }
        } else {
          ++stats.dropped;
        }
      };
    }

    auto wrapConsumer(auto&& fn, auto&& popOp) {
      return [fn = std::forward<decltype(fn)>(fn),
              popOp = std::forward<decltype(popOp)>(popOp),
              &stats = _stats](T& item, std::stop_token stop) -> bool {
        if (std::invoke(popOp, item, stop)) {
          ++stats.consumed;
          if (stats.queued > 0)
            --stats.queued;

          auto result = std::invoke(fn, std::move(item));

          if (result.has_value()) {
            ++stats.popped;
          } else {
            Log::logError(std::to_string(std::to_underlying(result.error())));
          }

          return true;
        }

        return false;
      };
    }

   private:
    PipeStats _stats{};
    std::optional<std::function<void(T)>> _deadLetter;
  };

  BoundedQueue<T> _queue;
  PipeMonitor _monitor;

  const PipePolicy _policy;
  const Duration _timeout;

  std::atomic<PipeState> _state{PipeState::Created};
  std::once_flag _init_flag{};
  std::stop_source _stopSource{};

  std::vector<PipeHandle> _ownedProducers{};
  std::vector<PipeHandle> _ownedConsumers{};
};
