#include "LockFreeStack.h"
#include "ThreadSafeStack.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// =============================================================================
//  Constants
// =============================================================================

static constexpr int kItems = 1024;  // ops per iteration in single-thread BMs
static constexpr int kMinThreads = 1;
static constexpr int kMaxThreads = 8;  // tune to physical core count

// =============================================================================
//  Shared prefill helper
// =============================================================================

template <typename Stack>
static void prefill(Stack& s, int n) {
  for (int i = 0; i < n; ++i)
    s.push(i);
}

// =============================================================================
//  SINGLE-THREADED benchmarks
// =============================================================================

// ── BM_Push_Single ───────────────────────────────────────────────────────────
// Measures raw push throughput with no contention.
template <typename Stack>
static void BM_Push_Single(benchmark::State& state) {
  Stack s;
  int val = 0;
  for (auto _ : state) {
    s.push(val++);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Push_Single, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Push/Single");
BENCHMARK_TEMPLATE(BM_Push_Single, LockFreeStack<int>)
    ->Name("LockFree/Push/Single");

// ── BM_Pop_Single ────────────────────────────────────────────────────────────
// Measures pop throughput on a pre-filled stack.
// Timing is paused during refill so only pop cost is measured.
template <typename Stack>
static void BM_Pop_Single(benchmark::State& state) {
  Stack s;
  for (auto _ : state) {
    state.PauseTiming();
    prefill(s, kItems);
    state.ResumeTiming();

    for (int i = 0; i < kItems; ++i)
      benchmark::DoNotOptimize(s.pop());
  }
  state.SetItemsProcessed(state.iterations() * kItems);
}

BENCHMARK_TEMPLATE(BM_Pop_Single, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Pop/Single");
BENCHMARK_TEMPLATE(BM_Pop_Single, LockFreeStack<int>)
    ->Name("LockFree/Pop/Single");

// ── BM_Top_Single ────────────────────────────────────────────────────────────
// Measures observer cost of top() — no mutation, pure read path.
template <typename Stack>
static void BM_Top_Single(benchmark::State& state) {
  Stack s;
  s.push(42);
  for (auto _ : state)
    benchmark::DoNotOptimize(s.top());
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Top_Single, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Top/Single");
BENCHMARK_TEMPLATE(BM_Top_Single, LockFreeStack<int>)
    ->Name("LockFree/Top/Single");

// ── BM_Empty_Single ──────────────────────────────────────────────────────────
// Measures empty() poll cost — relevant for spin-wait loops.
// Benchmarked on both an empty and a non-empty stack to expose any branch
// difference in the implementation.
template <typename Stack>
static void BM_Empty_Single_WhenEmpty(benchmark::State& state) {
  Stack s;
  for (auto _ : state)
    benchmark::DoNotOptimize(s.empty());
  state.SetItemsProcessed(state.iterations());
}

template <typename Stack>
static void BM_Empty_Single_WhenFull(benchmark::State& state) {
  Stack s;
  prefill(s, kItems);
  for (auto _ : state)
    benchmark::DoNotOptimize(s.empty());
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Empty_Single_WhenEmpty, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Empty/Single/Empty");
BENCHMARK_TEMPLATE(BM_Empty_Single_WhenEmpty, LockFreeStack<int>)
    ->Name("LockFree/Empty/Single/Empty");
BENCHMARK_TEMPLATE(BM_Empty_Single_WhenFull, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Empty/Single/Full");
BENCHMARK_TEMPLATE(BM_Empty_Single_WhenFull, LockFreeStack<int>)
    ->Name("LockFree/Empty/Single/Full");

// ── BM_Size_Single ───────────────────────────────────────────────────────────
// Measures size() cost — atomic load vs mutex-guarded read.
template <typename Stack>
static void BM_Size_Single(benchmark::State& state) {
  Stack s;
  prefill(s, kItems);
  for (auto _ : state)
    benchmark::DoNotOptimize(s.size());
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Size_Single, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Size/Single");
BENCHMARK_TEMPLATE(BM_Size_Single, LockFreeStack<int>)
    ->Name("LockFree/Size/Single");

// ── BM_PushPop_Single ────────────────────────────────────────────────────────
// Push immediately followed by pop: the fundamental round-trip latency.
// Best reveals lock overhead vs CAS overhead at zero contention.
template <typename Stack>
static void BM_PushPop_Single(benchmark::State& state) {
  Stack s;
  for (auto _ : state) {
    s.push(1);
    benchmark::DoNotOptimize(s.pop());
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_PushPop_Single, ThreadSafeStack<int>)
    ->Name("ThreadSafe/PushPop/Single");
BENCHMARK_TEMPLATE(BM_PushPop_Single, LockFreeStack<int>)
    ->Name("LockFree/PushPop/Single");

// =============================================================================
//  CONCURRENT benchmarks
//  state.threads() is driven by ->ThreadRange(); the stack is static so it
//  is shared across all benchmark threads within one run.
// =============================================================================

// ── BM_Push_Concurrent ───────────────────────────────────────────────────────
// All threads push simultaneously — pure write contention.
// Reveals how lock/CAS throughput degrades as thread count rises.
template <typename Stack>
static void BM_Push_Concurrent(benchmark::State& state) {
  static Stack s;

  // Thread 0 drains leftovers from the previous iteration
  if (state.thread_index() == 0) {
    while (s.pop()) {
    }
  }

  for (auto _ : state) {
    for (int i = 0; i < kItems; ++i)
      s.push(i);
  }
  state.SetItemsProcessed(state.iterations() * kItems);
}

BENCHMARK_TEMPLATE(BM_Push_Concurrent, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Push/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_Push_Concurrent, LockFreeStack<int>)
    ->Name("LockFree/Push/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();

// ── BM_Pop_Concurrent ────────────────────────────────────────────────────────
// All threads pop simultaneously — tests CAS retry storm vs mutex queue.
template <typename Stack>
static void BM_Pop_Concurrent(benchmark::State& state) {
  static Stack s;
  static std::atomic<int> ready{0};

  if (state.thread_index() == 0) {
    while (s.pop()) {
    }
    prefill(s, kItems * state.threads());
    ready.fetch_add(1, std::memory_order_release);
  }
  while (ready.load(std::memory_order_acquire) == 0)
    std::this_thread::yield();

  for (auto _ : state) {
    for (int i = 0; i < kItems; ++i)
      benchmark::DoNotOptimize(s.pop());
  }

  if (state.thread_index() == 0)
    ready.store(0, std::memory_order_release);

  state.SetItemsProcessed(state.iterations() * kItems);
}

BENCHMARK_TEMPLATE(BM_Pop_Concurrent, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Pop/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_Pop_Concurrent, LockFreeStack<int>)
    ->Name("LockFree/Pop/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();

// ── BM_Mixed_Concurrent ──────────────────────────────────────────────────────
// Even threads push, odd threads pop — classic producer/consumer split.
// The most realistic workload; tests scheduler fairness and throughput balance.
template <typename Stack>
static void BM_Mixed_Concurrent(benchmark::State& state) {
  static Stack s;

  const bool is_producer = (state.thread_index() % 2 == 0);

  for (auto _ : state) {
    if (is_producer) {
      for (int i = 0; i < kItems; ++i)
        s.push(i);
    } else {
      for (int i = 0; i < kItems; ++i)
        benchmark::DoNotOptimize(s.pop());
    }
  }
  state.SetItemsProcessed(state.iterations() * kItems);
}

BENCHMARK_TEMPLATE(BM_Mixed_Concurrent, ThreadSafeStack<int>)
    ->Name("ThreadSafe/Mixed/Concurrent")
    ->ThreadRange(2, kMaxThreads)  // minimum 2: need at least one of each role
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_Mixed_Concurrent, LockFreeStack<int>)
    ->Name("LockFree/Mixed/Concurrent")
    ->ThreadRange(2, kMaxThreads)
    ->UseRealTime();

// ── BM_PushPop_Concurrent ────────────────────────────────────────────────────
// Every thread alternates push→pop — maximum symmetric contention.
// Worst case for both mutex serialisation and CAS retry rates.
template <typename Stack>
static void BM_PushPop_Concurrent(benchmark::State& state) {
  static Stack s;

  for (auto _ : state) {
    for (int i = 0; i < kItems; ++i) {
      s.push(i);
      benchmark::DoNotOptimize(s.pop());
    }
  }
  state.SetItemsProcessed(state.iterations() * kItems);
}

BENCHMARK_TEMPLATE(BM_PushPop_Concurrent, ThreadSafeStack<int>)
    ->Name("ThreadSafe/PushPop/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_PushPop_Concurrent, LockFreeStack<int>)
    ->Name("LockFree/PushPop/Concurrent")
    ->ThreadRange(kMinThreads, kMaxThreads)
    ->UseRealTime();