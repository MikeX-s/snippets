#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "Pipe.h"

using namespace std::chrono_literals;

// ─── Helpers
// ──────────────────────────────────────────────────────────────────

// Bounded producer — stops after limit, then sleeps to avoid spinning
static auto makeProducer(std::atomic<int>& counter, int limit = INT_MAX) {
  return [&counter, limit]() -> std::expected<int, PipeError> {
    int expected = counter.load();
    while (expected < limit) {
      if (counter.compare_exchange_weak(expected, expected + 1))
        return expected + 1;
    }
    std::this_thread::sleep_for(1ms);
    return std::unexpected(PipeError::Closed);
  };
}

// Minimal consumer — counts received items
static auto makeConsumer(std::atomic<int>& counter) {
  return [&counter](int) -> std::expected<void, PipeError> {
    ++counter;
    return {};
  };
}

// Slow consumer — simulates processing lag
static auto makeSlowConsumer(std::atomic<int>& counter,
                             std::chrono::milliseconds delay) {
  return [&counter, delay](int) -> std::expected<void, PipeError> {
    std::this_thread::sleep_for(delay);
    ++counter;
    return {};
  };
}

// ─── Lifecycle
// ────────────────────────────────────────────────────────────────

TEST(PipeLifecycle, InitialStateIsCreated) {
  Pipe<int> pipe{16};
  EXPECT_FALSE(pipe.isRunning());
  EXPECT_FALSE(pipe.isClosed());
}

TEST(PipeLifecycle, ExplicitStartTransitionsToRunning) {
  Pipe<int> pipe{16};
  pipe.start();
  EXPECT_TRUE(pipe.isRunning());
  EXPECT_FALSE(pipe.isClosed());
}

TEST(PipeLifecycle, CloseTransitionsToClosed) {
  Pipe<int> pipe{16};
  pipe.start();
  pipe.close();
  EXPECT_FALSE(pipe.isRunning());
  EXPECT_TRUE(pipe.isClosed());
}

TEST(PipeLifecycle, CloseIsIdempotent) {
  Pipe<int> pipe{16};
  pipe.start();
  pipe.close();
  EXPECT_NO_THROW(pipe.close());
  EXPECT_TRUE(pipe.isClosed());
}

TEST(PipeLifecycle, LazyStartOnProduce) {
  Pipe<int> pipe{16};
  std::atomic<int> counter{0};
  auto handle = pipe.produce(makeProducer(counter));
  EXPECT_TRUE(pipe.isRunning());
  pipe.close();
}

TEST(PipeLifecycle, LazyStartOnConsume) {
  Pipe<int> pipe{16};
  std::atomic<int> counter{0};
  auto handle = pipe.consume(makeConsumer(counter));
  EXPECT_TRUE(pipe.isRunning());
  pipe.close();
}

TEST(PipeLifecycle, ExplicitAndLazyStartAreIdempotent) {
  Pipe<int> pipe{16};
  pipe.start();
  std::atomic<int> counter{0};
  auto handle = pipe.produce(makeProducer(counter));
  EXPECT_TRUE(pipe.isRunning());
  pipe.close();
}

// ─── PipeHandle
// ───────────────────────────────────────────────────────────────

TEST(PipeHandle, MoveTransfersOwnership) {
  Pipe<int> pipe{16};
  std::atomic<int> counter{0};
  auto handle = pipe.produce(makeProducer(counter));
  auto moved = std::move(handle);
  pipe.close();
}

TEST(PipeHandle, StopOnMovedFromIsNoop) {
  Pipe<int> pipe{16};
  std::atomic<int> counter{0};
  auto h1 = pipe.produce(makeProducer(counter));
  auto h2 = std::move(h1);
  EXPECT_NO_THROW(h1.stop());  // moved-from — noop
  h2.stop();                   // stop the actual live handle first
  pipe.close();
}

TEST(PipeHandle, StopCancelsWorker) {
  Pipe<int> pipe{64};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(2, makeProducer(produced));
  auto handle = pipe.consume(makeConsumer(consumed));

  std::this_thread::sleep_for(10ms);
  handle.stop();

  const int snapshot = consumed.load();
  std::this_thread::sleep_for(10ms);

  // After stop, this consumer no longer increments
  EXPECT_LE(consumed.load(), snapshot + 2);  // small slack for in-flight item
  pipe.close();
}

// ─── Produce / Consume
// ────────────────────────────────────────────────────────

TEST(PipeProduceConsume, ItemsFlowThrough) {
  Pipe<int> pipe{64};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(2, makeProducer(produced));
  pipe.consumeMany(2, makeConsumer(consumed));

  std::this_thread::sleep_for(20ms);
  pipe.close();

  EXPECT_GT(consumed.load(), 0);
}

TEST(PipeProduceConsume, BoundedProducerDrainsCleanly) {
  constexpr int N = 100;
  Pipe<int> pipe{128, PipePolicy::Block};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(2, makeProducer(produced, N));
  pipe.consumeMany(2, makeConsumer(consumed));

  std::this_thread::sleep_for(100ms);
  pipe.close();

  EXPECT_EQ(produced.load(), N);
  EXPECT_EQ(consumed.load(), N);
}

TEST(PipeProduceConsume, ProducerErrorSkipsEnqueue) {
  Pipe<int> pipe{16};
  std::atomic<int> consumed{0};

  auto ph = pipe.produce([]() -> std::expected<int, PipeError> {
    std::this_thread::sleep_for(1ms);
    return std::unexpected(PipeError::Dropped);
  });
  auto ch = pipe.consume(makeConsumer(consumed));

  std::this_thread::sleep_for(20ms);
  pipe.close();

  EXPECT_EQ(consumed.load(), 0);
}

TEST(PipeProduceConsume, ConsumerReceivesCorrectValues) {
  Pipe<int> pipe{64};
  std::atomic<int> sum{0};
  std::atomic<int> counter{0};

  auto ph = pipe.produce(makeProducer(counter, 10));
  auto ch = pipe.consume([&sum](int v) -> std::expected<void, PipeError> {
    sum += v;
    return {};
  });

  std::this_thread::sleep_for(50ms);
  pipe.close();

  EXPECT_EQ(sum.load(), 55);  // 1+2+...+10 = 55
}

// ─── Policy
// ───────────────────────────────────────────────────────────────────

TEST(PipePolicy, BlockStallsProducerWhenFull) {
  Pipe<int> pipe{4, PipePolicy::Block};
  std::atomic<int> produced{0};

  auto ph = pipe.produce(makeProducer(produced));

  std::this_thread::sleep_for(20ms);
  EXPECT_LE(produced.load(), 8);  // capped near capacity
  pipe.close();
}

TEST(PipePolicy, DropNewestDiscardsIncomingWhenFull) {
  Pipe<int> pipe{8, PipePolicy::DropNewest};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(4, makeProducer(produced));
  pipe.consumeMany(1, makeSlowConsumer(consumed, 2ms));

  std::this_thread::sleep_for(50ms);
  pipe.close();

  EXPECT_LT(consumed.load(), produced.load());
}

TEST(PipePolicy, DropOldestEvictsHeadWhenFull) {
  Pipe<int> pipe{4, PipePolicy::DropOldest};
  std::atomic<int> lastSeen{0};
  std::atomic<int> counter{0};

  auto ph = pipe.produce(makeProducer(counter, 20));
  auto ch = pipe.consume([&lastSeen](int v) -> std::expected<void, PipeError> {
    lastSeen.store(v, std::memory_order_relaxed);
    return {};
  });

  std::this_thread::sleep_for(30ms);
  pipe.close();

  EXPECT_GT(lastSeen.load(), 1);
}

// ─── Cancellation
// ─────────────────────────────────────────────────────────────

TEST(PipeCancellation, CloseStopsAllWorkers) {
  Pipe<int> pipe{16};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(4, makeProducer(produced));
  pipe.consumeMany(4, makeConsumer(consumed));

  std::this_thread::sleep_for(10ms);
  pipe.close();

  const int snapshot = consumed.load();
  std::this_thread::sleep_for(10ms);

  EXPECT_EQ(consumed.load(), snapshot);
}

TEST(PipeCancellation, DestructorCallsClose) {
  std::atomic<int> consumed{0};
  {
    Pipe<int> pipe{16};
    std::atomic<int> produced{0};
    pipe.produceMany(2, makeProducer(produced));
    pipe.consumeMany(2, makeConsumer(consumed));
    std::this_thread::sleep_for(5ms);
  }  // destructor fires here — no crash, no hang
  EXPECT_GT(consumed.load(), 0);
}

// ─── Stats
// ────────────────────────────────────────────────────────────────────

TEST(PipeStats, PushedAndPoppedCountedCorrectly) {
  constexpr int N = 100;
  Pipe<int> pipe{256};
  std::atomic<int> counter{0}, consumed{0};

  pipe.produceMany(2, makeProducer(counter, N));
  pipe.consumeMany(2, makeConsumer(consumed));

  std::this_thread::sleep_for(100ms);
  pipe.close();

  const auto& s = pipe.getStats();
  EXPECT_EQ(s.pushed.load(), N);
  EXPECT_EQ(s.popped.load(), N);
  EXPECT_GE(s.produced.load(), N);
}

TEST(PipeStats, DroppedCountedUnderDropNewest) {
  Pipe<int> pipe{8, PipePolicy::DropNewest};
  std::atomic<int> produced{0}, consumed{0};

  pipe.produceMany(4, makeProducer(produced));
  pipe.consumeMany(1, makeSlowConsumer(consumed, 5ms));

  std::this_thread::sleep_for(50ms);
  pipe.close();

  const auto& s = pipe.getStats();
  EXPECT_GT(s.dropped.load(), 0);
  EXPECT_EQ(s.pushed.load() + s.dropped.load(), s.produced.load());
}

TEST(PipeStats, ProducedCountsAllInvocationsIncludingErrors) {
  Pipe<int> pipe{16};

  auto ph = pipe.produce([]() -> std::expected<int, PipeError> {
    std::this_thread::sleep_for(1ms);
    return std::unexpected(PipeError::Dropped);
  });
  // no consumer — nothing can consume, nothing can push successfully

  std::this_thread::sleep_for(20ms);
  pipe.close();

  const auto& s = pipe.getStats();
  EXPECT_GT(s.produced.load(), 0);
  EXPECT_EQ(s.pushed.load(), 0);
  EXPECT_EQ(s.dropped.load(), s.produced.load());
}

// ─── Concepts
// ─────────────────────────────────────────────────────────────────

TEST(PipeConcepts, ProducerConceptAcceptsCorrectSignature) {
  Pipe<int> pipe{16};
  auto h = pipe.produce([]() -> std::expected<int, PipeError> { return 42; });
  pipe.close();
}

TEST(PipeConcepts, ConsumerConceptAcceptsCorrectSignature) {
  Pipe<int> pipe{16};
  auto h =
      pipe.consume([](int) -> std::expected<void, PipeError> { return {}; });
  pipe.close();
}

// ─── Stress
// ───────────────────────────────────────────────────────────────────

TEST(PipeStress, ManyProducersManyConsumers) {
  constexpr int N = 1000;
  Pipe<int> pipe{256, PipePolicy::Block};
  std::atomic<int> counter{0}, consumed{0};

  pipe.produceMany(8, makeProducer(counter, N));
  pipe.consumeMany(8, makeConsumer(consumed));

  std::this_thread::sleep_for(200ms);
  pipe.close();

  EXPECT_EQ(counter.load(), N);
  EXPECT_EQ(consumed.load(), N);
}

TEST(PipeStress, NoDataRaceOnRepeatedClose) {
  for (int i = 0; i < 20; ++i) {
    Pipe<int> pipe{32, PipePolicy::DropOldest};
    std::atomic<int> dummy{0};

    pipe.produceMany(4, makeProducer(dummy));
    pipe.consumeMany(4, makeConsumer(dummy));

    std::this_thread::sleep_for(1ms);
    pipe.close();
  }
}

TEST(PipeStress, StatsConsistentUnderLoad) {
  constexpr int N = 2000;
  Pipe<int> pipe{64, PipePolicy::Block};
  std::atomic<int> counter{0}, consumed{0};

  pipe.produceMany(4, makeProducer(counter, N / 4));
  pipe.consumeMany(4, makeConsumer(consumed));

  std::this_thread::sleep_for(200ms);
  pipe.close();

  const auto& s = pipe.getStats();
  EXPECT_EQ(s.pushed.load() + s.dropped.load(), s.produced.load());
  EXPECT_EQ(s.pushed.load(), s.popped.load());
}