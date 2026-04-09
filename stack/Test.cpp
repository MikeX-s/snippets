#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

#include "LockFreeStack.h"
#include "ThreadSafeStack.h"

struct MoveOnly {
  explicit MoveOnly(int val) : value{val} {}

  MoveOnly(const MoveOnly& other) = delete;
  MoveOnly& operator=(const MoveOnly& other) = delete;
  MoveOnly(MoveOnly&& other) = default;
  MoveOnly& operator=(MoveOnly&& other) = default;

  int value;
};

struct BigChunk {
  std::array<uint64_t, 128> data{};
  uint64_t sentinel = 0xDEADBEEFCAFEBABE;
};

// --- Fixtures ---

template <typename T>
class StackIntTest : public ::testing::Test {};
template <typename T>
class StackBigChunkTest : public ::testing::Test {};
template <typename T>
class StackMoveOnlyTest : public ::testing::Test {};

using IntTypes = ::testing::Types<LockFreeStack<int>, ThreadSafeStack<int>>;
using BigChunkTypes =
    ::testing::Types<LockFreeStack<BigChunk>, ThreadSafeStack<BigChunk>>;
using MoveOnlyTypes =
    ::testing::Types<LockFreeStack<MoveOnly>, ThreadSafeStack<MoveOnly>>;

TYPED_TEST_SUITE(StackIntTest, IntTypes);
TYPED_TEST_SUITE(StackBigChunkTest, BigChunkTypes);
TYPED_TEST_SUITE(StackMoveOnlyTest, MoveOnlyTypes);

TYPED_TEST(StackIntTest, PushUpdatesStateCorrectly) {
  TypeParam s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);

  s.push(1);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 1u);

  s.push(2);
  EXPECT_EQ(s.size(), 2u);
}

TYPED_TEST(StackIntTest, TryPopRefReturnsLIFOOrder) {
  TypeParam s;
  s.push(10);
  s.push(20);
  s.push(30);

  int out = 0;
  EXPECT_EQ(s.top(), 30);
  s.pop();
  EXPECT_EQ(s.top(), 20);
  s.pop();
  EXPECT_EQ(s.top(), 10);
  s.pop();

  EXPECT_TRUE(s.empty());
}

TYPED_TEST(StackIntTest, TopPeeksWithoutRemoving) {
  TypeParam s;
  s.push(42);
  s.push(99);

  EXPECT_EQ(s.top(), 99);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(s.top(), 99);
}

TYPED_TEST(StackIntTest, TopOnEmptyStackThrows) {
  TypeParam s;
  EXPECT_THROW(s.top(), std::out_of_range);
}

TYPED_TEST(StackIntTest, CopyConstructorIsDeepCopy) {
  TypeParam original;
  original.push(1);
  original.push(2);

  TypeParam copy(original);

  // Drain the original
  original.pop();
  original.pop();
  EXPECT_TRUE(original.empty());

  // Copy is unaffected
  EXPECT_EQ(copy.size(), 2u);
  EXPECT_EQ(copy.top(), 2);
}

TYPED_TEST(StackMoveOnlyTest, PushAcceptsMoveOnlyType) {
  TypeParam s;
  s.push(MoveOnly{42});

  EXPECT_EQ(s.top().value, 42);
}

TYPED_TEST(StackIntTest, SingleElementRoundTrip) {
  TypeParam s;
  s.push(1);

  ASSERT_EQ(*s.pop(), 1);
  EXPECT_TRUE(s.empty());
  // Second pop on now-empty stack must be safe
  EXPECT_EQ(s.pop(), nullptr);
}

TYPED_TEST(StackBigChunkTest, LargePayloadNoConcurrentCorruption) {
  TypeParam s;
  constexpr int N = 200;
  std::thread producer([&] {
    for (int i = 0; i < N; ++i)
      s.push(BigChunk{});
  });

  std::atomic<int> bad{0};
  std::thread consumer([&] {
    int popped = 0;
    while (popped < N) {
      if (auto chunk = s.pop(); chunk != nullptr) {
        if (chunk->sentinel != 0xDEADBEEFCAFEBABE) {
          ++bad;
        }
        ++popped;
      }
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(bad.load(), 0);
}

TYPED_TEST(StackIntTest, StressTestConsistentFinalState) {
  TypeParam s;
  constexpr int N_THREADS = 32;
  constexpr int OPS_PER_THREAD = 3125;  // 32 * 3125 = 100 000 pushes
  std::atomic<int> push_count{0}, pop_count{0};

  std::vector<std::thread> threads;
  threads.reserve(N_THREADS * 2);

  for (int t = 0; t < N_THREADS; ++t)
    threads.emplace_back([&, t] {
      for (int i = 0; i < OPS_PER_THREAD; ++i) {
        s.push(t * OPS_PER_THREAD + i);
        ++push_count;
      }
    });

  for (int t = 0; t < N_THREADS; ++t)
    threads.emplace_back([&] {
      for (int i = 0; i < OPS_PER_THREAD; ++i) {
        // spin until we get one (producers might lag)
        while (!s.pop()) {
          std::this_thread::yield();
        }
        ++pop_count;
      }
    });

  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(push_count.load(), N_THREADS * OPS_PER_THREAD);
  EXPECT_EQ(pop_count.load(), N_THREADS * OPS_PER_THREAD);
  // Remaining items = pushed − popped
  EXPECT_EQ(s.size(), static_cast<std::size_t>(push_count - pop_count));
}

// =============================================================================
//  Thread Safety
// =============================================================================

TYPED_TEST(StackIntTest, ConcurrentPushesProduceExactCount) {
  TypeParam s;
  constexpr int N_THREADS = 8;
  constexpr int N_ITEMS = 1000;

  std::vector<std::thread> threads;
  threads.reserve(N_THREADS);
  for (int t = 0; t < N_THREADS; ++t) {
    threads.emplace_back([&s, t] {
      for (int i = 0; i < N_ITEMS; ++i)
        s.push(t * N_ITEMS + i);
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(s.size(), static_cast<std::size_t>(N_THREADS * N_ITEMS));
}

TYPED_TEST(StackIntTest, ConcurrentPopsNoDuplicatesNoLoss) {
  TypeParam s;
  constexpr int TOTAL = 10'000;
  constexpr int N_THREADS = 8;
  std::atomic<int> popped_count{0};

  std::vector<std::thread> threads;
  threads.reserve(N_THREADS);

  for (int i = 0; i < TOTAL; ++i) {
    s.push(i);
  }

  for (int t = 0; t < N_THREADS; ++t) {
    threads.emplace_back([&] {
      while (s.pop()) {
        ++popped_count;
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(popped_count.load(), TOTAL);
  EXPECT_TRUE(s.empty());
}

TYPED_TEST(StackIntTest, MixedProducersConsumersNoDataLoss) {
  TypeParam s;
  constexpr int N_PRODUCERS = 4;
  constexpr int N_CONSUMERS = 4;
  constexpr int ITEMS_EACH = 2500;
  constexpr int TOTAL = N_PRODUCERS * ITEMS_EACH;

  std::atomic<int> produced{0}, consumed{0};
  auto producer = [&] {
    for (int i = 0; i < ITEMS_EACH; ++i) {
      s.push(i);
      ++produced;
    }
  };

  auto consumer = [&] {
    while (s.pop()) {
      ++consumed;
      if (produced.load() == TOTAL && s.empty()) {
        break;
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < N_PRODUCERS; ++i) {
    threads.emplace_back(producer);
  }
  for (int i = 0; i < N_CONSUMERS; ++i) {
    threads.emplace_back(consumer);
  }
  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(consumed.load(), TOTAL);
}

TYPED_TEST(StackIntTest, StackDestructionAfterDrainIsSafe) {
  std::atomic<int> consumed{0};
  constexpr int TOTAL = 500;
  {
    TypeParam s;
    for (int i = 0; i < TOTAL; ++i) {
      s.push(i);
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
      threads.emplace_back([&] {
        while (s.pop()) {
          ++consumed;
        }
      });

    for (auto& th : threads) {
      th.join();
    }
    // `s` goes out of scope here – must not crash
  }
  EXPECT_EQ(consumed.load(), TOTAL);
}
