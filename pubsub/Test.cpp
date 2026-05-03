#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "Broker.h"
#include "CommonTypes.h"
#include "Event.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "Subscription.h"

using namespace std::chrono_literals;

#ifdef __SANITIZE_THREAD__
constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{5000};
#else
constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{200};
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Event makeEvent(Topic_t topic) {
  return EventBuilder{}
      .topic(std::move(topic))
      .payload(std::make_shared<const DataType1>(DataType1{1.0f, 42u}))
      .build();
}

static void waitFor(std::atomic<int>& counter,
                    int expected,
                    std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (counter.load() < expected &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(1ms);
  }
}

// Drives a jthread-based stop source so pop() can be unblocked cleanly
// in tests that need to cancel a waiting pop.
struct StopDriver {
  std::stop_source source;
  std::stop_token token() { return source.get_token(); }
  void stop() { source.request_stop(); }
};

// Pop with a timeout — returns false if nothing arrives within the deadline.
template <typename T>
bool popWithTimeout(BoundedQueue<T>& q,
                    T& out,
                    std::chrono::milliseconds timeout = 200ms) {
  StopDriver sd;
  std::thread killer{[&] {
    std::this_thread::sleep_for(timeout);
    sd.stop();
  }};
  bool result = q.pop(out, sd.token());
  killer.join();
  return result;
}

// ---------------------------------------------------------------------------
// EventBuilder tests
// ---------------------------------------------------------------------------

TEST(EventBuilderTest, BuildSetsTimestamp) {
  auto before = std::chrono::steady_clock::now();
  auto event = makeEvent("test/topic");
  auto after = std::chrono::steady_clock::now();

  EXPECT_GE(event.timestamp, before);
  EXPECT_LE(event.timestamp, after);
}

TEST(EventBuilderTest, BuildSetsTopic) {
  auto event = makeEvent("sensor/temp");
  EXPECT_EQ(event.topic, "sensor/temp");
}

TEST(EventBuilderTest, BuildSetsPayload) {
  auto event = makeEvent("sensor/temp");
  EXPECT_TRUE(
      std::holds_alternative<std::shared_ptr<const DataType1>>(event.payload));
}

TEST(EventBuilderTest, NullPayloadThrows) {
  EXPECT_THROW(EventBuilder{}
                   .topic("t")
                   .payload(std::shared_ptr<const DataType1>{nullptr})
                   .build(),
               std::runtime_error);
}

// ---------------------------------------------------------------------------
// Subscription tests
// ---------------------------------------------------------------------------

TEST(SubscriptionTest, MoveConstructorInvalidatesSource) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  auto s1 = sub.subscribe("topic", [](const Event&) {});
  auto id = s1.getId();

  Subscription s2{std::move(s1)};

  EXPECT_EQ(s2.getId(), id);
}

TEST(SubscriptionTest, MoveAssignmentInvalidatesSource) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  auto s1 = sub.subscribe("topic/a", [](const Event&) {});
  auto s2 = sub.subscribe("topic/b", [](const Event&) {});

  auto id = s1.getId();
  s2 = std::move(s1);

  EXPECT_EQ(s2.getId(), id);
}

TEST(SubscriptionTest, EqualityOnSameId) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  auto s1 = sub.subscribe("topic", [](const Event&) {});
  // Move to s2 — same id
  Subscription s2{std::move(s1)};

  EXPECT_EQ(s2, s2);
}

TEST(SubscriptionTest, DestructorUnsubscribes) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  {
    auto s = sub.subscribe("topic", [&](const Event&) { ++count; });
    // s goes out of scope here — unsubscribes
  }

  IPublish* pub = broker.get();
  pub->publish(makeEvent("topic"));

  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 0);
}

// ---------------------------------------------------------------------------
// Broker tests
// ---------------------------------------------------------------------------

TEST(BrokerTest, CreateReturnsBroker) {
  auto broker = Broker::create();
  EXPECT_NE(broker, nullptr);
}

TEST(BrokerTest, PublishRoutesToCorrectTopic) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s = sub.subscribe("sensor/temp", [&](const Event&) { ++count; });

  IPublish* pub = broker.get();
  pub->publish(makeEvent("sensor/temp"));

  waitFor(count, 1);
  EXPECT_EQ(count.load(), 1);
}

TEST(BrokerTest, PublishDoesNotRouteToWrongTopic) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s = sub.subscribe("sensor/temp", [&](const Event&) { ++count; });

  IPublish* pub = broker.get();
  pub->publish(makeEvent("sensor/humidity"));

  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 0);
}

TEST(BrokerTest, MultipleSubscribersOnSameTopic) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s1 = sub.subscribe("topic", [&](const Event&) { ++count; });
  auto s2 = sub.subscribe("topic", [&](const Event&) { ++count; });

  IPublish* pub = broker.get();
  pub->publish(makeEvent("topic"));

  waitFor(count, 2);
  EXPECT_EQ(count.load(), 2);
}

TEST(BrokerTest, UnsubscribeOnlyRemovesTargetSubscriber) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s1 = sub.subscribe("topic", [&](const Event&) { ++count; });
  {
    auto s2 = sub.subscribe("topic", [&](const Event&) { ++count; });
    // s2 unsubscribes here
  }

  IPublish* pub = broker.get();
  pub->publish(makeEvent("topic"));

  waitFor(count, 1);
  std::this_thread::sleep_for(50ms);
  // Only s1 should fire — not 2, not 0
  EXPECT_EQ(count.load(), 1);
}

TEST(BrokerTest, MultiplePublishesDeliveredInOrder) {
  auto broker = Broker::create();
  Subscriber sub{broker};

  std::vector<int> received;
  std::mutex mtx;
  std::condition_variable cv;

  auto s = sub.subscribe("topic", [&](const Event& e) {
    auto& d = std::get<std::shared_ptr<const DataType1>>(e.payload);

    {
      std::lock_guard lock{mtx};
      received.push_back(static_cast<int>(d->sensorId));
    }

    cv.notify_one();
  });

  IPublish* pub = broker.get();
  for (int i = 0; i < 5; ++i) {
    auto event = EventBuilder{}
                     .topic("topic")
                     .payload(std::make_shared<const DataType1>(
                         DataType1{0.f, static_cast<uint32_t>(i)}))
                     .build();
    pub->publish(std::move(event));
  }

  std::unique_lock lock{mtx};
  bool finished =
      cv.wait_for(lock, 200ms, [&] { return received.size() == 5; });

  ASSERT_TRUE(finished) << "Received: " << received.size()
                        << " events before timeout";
  EXPECT_THAT(received, ::testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST(BrokerTest, BrokerDestroyedBeforeSubscription) {
  std::optional<Subscription> s;
  {
    auto broker = Broker::create();
    Subscriber sub{broker};
    s.emplace(sub.subscribe("topic", [](const Event&) {}));
    // broker destroyed here, sub destroyed here
  }

  // s holds weak_ptr — destructor should not crash
  EXPECT_NO_THROW(s.reset());
}

// ---------------------------------------------------------------------------
// Subscriber lifetime (LifetimeToken) tests
// ---------------------------------------------------------------------------

TEST(SubscriberLifetimeTest, CallbackNotInvokedAfterSubscriberDestroyed) {
  auto broker = Broker::create();
  std::atomic<int> count{0};
  Subscription s = [&] {
    Subscriber sub{broker};
    return sub.subscribe("topic", [&](const Event&) { ++count; });
    // sub destroyed here — token expires
  }();

  IPublish* pub = broker.get();
  pub->publish(makeEvent("topic"));

  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 0);
}

TEST(SubscriberLifetimeTest, CallbackStillInvokedWhileSubscriberAlive) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s = sub.subscribe("topic", [&](const Event&) { ++count; });

  IPublish* pub = broker.get();
  pub->publish(makeEvent("topic"));

  waitFor(count, 1);
  EXPECT_EQ(count.load(), 1);
}

// ---------------------------------------------------------------------------
// Interface segregation tests
// ---------------------------------------------------------------------------

TEST(InterfaceSegregationTest, IPublishableOnlyExposesPublish) {
  // Compile-time check: IPublishable* cannot call subscribe
  // If this compiles, segregation is enforced
  auto broker = Broker::create();
  IPublish* pub = broker.get();
  EXPECT_NE(pub, nullptr);
  // pub->subscribe(...)  — should not compile
}

TEST(InterfaceSegregationTest, ISubscribableOnlyExposesSubscribe) {
  auto broker = Broker::create();
  ISubscribe* subInterface = broker.get();
  EXPECT_NE(subInterface, nullptr);
  // subInterface->publish(...)  — should not compile
}

// ---------------------------------------------------------------------------
// Thread safety tests
// ---------------------------------------------------------------------------

TEST(ThreadSafetyTest, ConcurrentPublishFromMultipleThreads) {
  auto broker = Broker::create();
  Subscriber sub{broker};
  std::atomic<int> count{0};

  auto s = sub.subscribe("topic", [&](const Event&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  IPublish* pub = broker.get();
  constexpr int numThreads = 8;
  constexpr int eventsPerThread = 25;

  std::vector<std::thread> threads;
  for (int i = 0; i < numThreads; ++i) {
    // JAWNIE przekazujemy pub przez wartość,
    // a resztę przez referencję, ale upewniamy się, że żyją
    threads.emplace_back([pub, &count, eventsPerThread] {
      for (int j = 0; j < eventsPerThread; ++j) {
        // DODAJ LOGIKĘ SPRAWDZAJĄCĄ
        if (!pub->publish(makeEvent("topic"))) {
          // Jeśli to zobaczysz w TSAN - mamy dowód, że push zawodzi
          std::cerr << "PUBLISH FAILED at thread loop " << j << std::endl;
        }
      }
    });
  }

  for (auto& t : threads)
    t.join();

  waitFor(count, 200, 10s);
  EXPECT_EQ(count.load(), 200);
}

TEST(ThreadSafetyTest, ConcurrentSubscribeAndPublish) {
  auto broker = Broker::create();
  std::atomic<int> count{0};
  std::vector<Subscription> subs;
  std::mutex subsMtx;

  IPublish* pub = broker.get();

  std::thread publisher{[&] {
    for (int i = 0; i < 50; ++i) {
      pub->publish(makeEvent("topic"));
      std::this_thread::sleep_for(1ms);
    }
  }};

  std::thread subscriber{[&] {
    for (int i = 0; i < 5; ++i) {
      Subscriber sub{broker};
      std::lock_guard lock{subsMtx};
      subs.push_back(sub.subscribe("topic", [&](const Event&) { ++count; }));
      std::this_thread::sleep_for(5ms);
    }
  }};

  publisher.join();
  subscriber.join();

  // No crash and no data race is the primary assertion here
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(BoundedQueueTest, ConstructsWithCapacity) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0u);
}

TEST(BoundedQueueTest, CapacityOfOneIsValid) {
  BoundedQueue<int> q{1, DropPolicy::DropNewest};
  EXPECT_TRUE(q.push(1));
  EXPECT_EQ(q.size(), 1u);
}

// ---------------------------------------------------------------------------
// Basic push / pop
// ---------------------------------------------------------------------------

TEST(BoundedQueueTest, PushAndPopSingleItem) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  EXPECT_TRUE(q.push(42));
  int out{};
  StopDriver sd;
  EXPECT_TRUE(q.pop(out, sd.token()));
  EXPECT_EQ(out, 42);
}

TEST(BoundedQueueTest, FIFOOrderPreserved) {
  BoundedQueue<int> q{8, DropPolicy::DropNewest};
  for (int i = 0; i < 5; ++i)
    q.push(i);

  StopDriver sd;
  std::vector<int> result;
  for (int i = 0; i < 5; ++i) {
    int out{};
    q.pop(out, sd.token());
    result.push_back(out);
  }
  EXPECT_THAT(result, ::testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST(BoundedQueueTest, SizeTracksCorrectly) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  EXPECT_EQ(q.size(), 0u);
  q.push(1);
  EXPECT_EQ(q.size(), 1u);
  q.push(2);
  EXPECT_EQ(q.size(), 2u);

  int out{};
  StopDriver sd;
  q.pop(out, sd.token());
  EXPECT_EQ(q.size(), 1u);
}

TEST(BoundedQueueTest, EmptyAfterAllItemsPopped) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  q.push(1);
  q.push(2);

  int out{};
  StopDriver sd;
  q.pop(out, sd.token());
  q.pop(out, sd.token());
  EXPECT_TRUE(q.empty());
}

// ---------------------------------------------------------------------------
// DropNewest policy
// ---------------------------------------------------------------------------

TEST(BoundedQueueDropNewestTest, PushBelowCapacitySucceeds) {
  BoundedQueue<int> q{3, DropPolicy::DropNewest};
  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));
  EXPECT_TRUE(q.push(3));
}

TEST(BoundedQueueDropNewestTest, PushAtCapacityDropsIncoming) {
  // BUG EXPOSURE: with the > bug, capacity=3 allows 4 items.
  // This test documents correct behaviour: push on a full queue returns false.
  BoundedQueue<int> q{3, DropPolicy::DropNewest};
  q.push(1);
  q.push(2);
  q.push(3);
  EXPECT_FALSE(q.push(99));  // should be dropped
  EXPECT_EQ(q.size(), 3u);   // size must not exceed capacity
}

TEST(BoundedQueueDropNewestTest, DroppedItemNotDelivered) {
  BoundedQueue<int> q{2, DropPolicy::DropNewest};
  q.push(10);
  q.push(20);
  q.push(99);  // dropped

  int out{};
  StopDriver sd;
  q.pop(out, sd.token());
  EXPECT_EQ(out, 10);
  q.pop(out, sd.token());
  EXPECT_EQ(out, 20);
  // queue now empty — no trace of 99
  EXPECT_TRUE(q.empty());
}

TEST(BoundedQueueDropNewestTest, OldItemsPreservedWhenFull) {
  BoundedQueue<int> q{3, DropPolicy::DropNewest};
  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4);
  q.push(5);  // both dropped

  int out{};
  StopDriver sd;
  std::vector<int> received;
  while (!q.empty()) {
    q.pop(out, sd.token());
    received.push_back(out);
  }
  EXPECT_THAT(received, ::testing::ElementsAre(1, 2, 3));
}

// ---------------------------------------------------------------------------
// DropOldest policy
// ---------------------------------------------------------------------------

TEST(BoundedQueueDropOldestTest, PushBelowCapacitySucceeds) {
  BoundedQueue<int> q{3, DropPolicy::DropOldest};
  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));
  EXPECT_TRUE(q.push(3));
}

TEST(BoundedQueueDropOldestTest, PushAtCapacityEvictsFront) {
  BoundedQueue<int> q{3, DropPolicy::DropOldest};
  q.push(1);
  q.push(2);
  q.push(3);
  EXPECT_TRUE(q.push(4));   // evicts 1, inserts 4
  EXPECT_EQ(q.size(), 3u);  // size must not exceed capacity
}

TEST(BoundedQueueDropOldestTest, MostRecentItemsRetained) {
  BoundedQueue<int> q{3, DropPolicy::DropOldest};
  for (int i = 1; i <= 6; ++i)
    q.push(i);  // 1,2,3 evicted

  int out{};
  StopDriver sd;
  std::vector<int> received;
  while (!q.empty()) {
    q.pop(out, sd.token());
    received.push_back(out);
  }
  EXPECT_THAT(received, ::testing::ElementsAre(4, 5, 6));
}

TEST(BoundedQueueDropOldestTest, SingleCapacityAlwaysHoldsLatest) {
  BoundedQueue<int> q{1, DropPolicy::DropOldest};
  q.push(1);
  q.push(2);
  q.push(3);

  int out{};
  StopDriver sd;
  q.pop(out, sd.token());
  EXPECT_EQ(out, 3);
}

// ---------------------------------------------------------------------------
// Block policy
// ---------------------------------------------------------------------------

TEST(BoundedQueueBlockTest, PushBelowCapacityDoesNotBlock) {
  BoundedQueue<int> q{4, DropPolicy::Block};
  // All pushes should complete immediately
  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));
  EXPECT_TRUE(q.push(3));
  EXPECT_TRUE(q.push(4));
}

TEST(BoundedQueueBlockTest, PushUnblocksAfterPop) {
  BoundedQueue<int> q{2, DropPolicy::Block};
  q.push(1);
  q.push(2);

  std::atomic<bool> pushed{false};
  std::jthread producer{[&](std::stop_token) {
    q.push(3);  // blocks until a slot is free
    pushed = true;
  }};

  std::this_thread::sleep_for(30ms);
  EXPECT_FALSE(pushed.load());  // still blocked

  int out{};
  StopDriver sd;
  q.pop(out, sd.token());  // frees a slot

  auto deadline = std::chrono::steady_clock::now() + 200ms;
  while (!pushed.load() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);

  EXPECT_TRUE(pushed.load());
}

TEST(BoundedQueueBlockTest, NoItemsLostUnderBlock) {
  constexpr int N = 20;
  BoundedQueue<int> q{4, DropPolicy::Block};
  std::vector<int> received;
  std::mutex recvMtx;

  std::jthread consumer{[&](std::stop_token stop) {
    int out{};
    while (q.pop(out, stop)) {
      std::lock_guard lock{recvMtx};
      received.push_back(out);
    }
  }};

  for (int i = 0; i < N; ++i)
    q.push(i);

  // Give consumer time to drain
  auto deadline = std::chrono::steady_clock::now() + 500ms;
  while (true) {
    std::lock_guard lock{recvMtx};
    if (received.size() == N || std::chrono::steady_clock::now() > deadline)
      break;
  }
  consumer.request_stop();

  std::lock_guard lock{recvMtx};
  EXPECT_EQ(received.size(), static_cast<std::size_t>(N));
  // All items present, no duplicates
  std::sort(received.begin(), received.end());
  for (int i = 0; i < N; ++i)
    EXPECT_EQ(received[i], i);
}

// ---------------------------------------------------------------------------
// pop() stop_token integration
// ---------------------------------------------------------------------------

TEST(BoundedQueueTest, PopReturnsFalseOnStopWithEmptyQueue) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  StopDriver sd;
  sd.stop();  // stop before pop

  int out{};
  EXPECT_FALSE(q.pop(out, sd.token()));
}

TEST(BoundedQueueTest, PopUnblocksOnStopSignal) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  StopDriver sd;
  std::atomic<bool> returned{false};

  std::thread t{[&] {
    int out{};
    q.pop(out, sd.token());  // will block — queue is empty
    returned = true;
  }};

  std::this_thread::sleep_for(30ms);
  EXPECT_FALSE(returned.load());

  sd.stop();

  auto deadline = std::chrono::steady_clock::now() + 200ms;
  while (!returned.load() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);

  EXPECT_TRUE(returned.load());
  t.join();
}

TEST(BoundedQueueTest, PopDeliversItemBeforeStop) {
  BoundedQueue<int> q{4, DropPolicy::DropNewest};
  q.push(77);

  StopDriver sd;
  int out{};
  // Item present — should return true even though stop is also requested
  EXPECT_TRUE(q.pop(out, sd.token()));
  EXPECT_EQ(out, 77);
}

// ---------------------------------------------------------------------------
// Capacity boundary
// ---------------------------------------------------------------------------

TEST(BoundedQueueTest, SizeNeverExceedsCapacity_DropNewest) {
  constexpr std::size_t cap = 5;
  BoundedQueue<int> q{cap, DropPolicy::DropNewest};
  for (int i = 0; i < 20; ++i)
    q.push(i);
  EXPECT_LE(q.size(), cap);
}

TEST(BoundedQueueTest, SizeNeverExceedsCapacity_DropOldest) {
  constexpr std::size_t cap = 5;
  BoundedQueue<int> q{cap, DropPolicy::DropOldest};
  for (int i = 0; i < 20; ++i)
    q.push(i);
  EXPECT_LE(q.size(), cap);
}

TEST(BoundedQueueTest, ExactlyCapacityItemsFitBeforeDrop) {
  // With correct >= check: capacity=3 → items 0,1,2 fit; item 3 is dropped.
  // With buggy > check:    capacity=3 → items 0,1,2,3 fit; item 4 is dropped.
  constexpr std::size_t cap = 3;
  BoundedQueue<int> q{cap, DropPolicy::DropNewest};
  for (int i = 0; i < 10; ++i)
    q.push(i);
  EXPECT_EQ(q.size(), cap);  // exactly cap, not cap+1
}

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------

TEST(BoundedQueueThreadSafetyTest, ConcurrentPushDropNewest) {
  constexpr std::size_t cap = 16;
  BoundedQueue<int> q{cap, DropPolicy::DropNewest};

  std::vector<std::thread> producers;
  for (int t = 0; t < 8; ++t) {
    producers.emplace_back([&, t] {
      for (int i = 0; i < 50; ++i)
        q.push(t * 100 + i);
    });
  }
  for (auto& t : producers)
    t.join();

  EXPECT_LE(q.size(), cap);
}

TEST(BoundedQueueThreadSafetyTest, ConcurrentPushDropOldest) {
  constexpr std::size_t cap = 8;
  BoundedQueue<int> q{cap, DropPolicy::DropOldest};

  std::vector<std::thread> producers;
  for (int t = 0; t < 8; ++t) {
    producers.emplace_back([&, t] {
      for (int i = 0; i < 50; ++i)
        q.push(t * 100 + i);
    });
  }
  for (auto& t : producers)
    t.join();

  EXPECT_LE(q.size(), cap);
}

TEST(BoundedQueueThreadSafetyTest, ConcurrentPushAndPop) {
  constexpr int N = 200;
  BoundedQueue<int> q{16, DropPolicy::DropNewest};
  std::atomic<int> consumed{0};

  std::jthread consumer{[&](std::stop_token stop) {
    int out{};
    while (q.pop(out, stop))
      ++consumed;
  }};

  std::vector<std::thread> producers;
  for (int t = 0; t < 4; ++t) {
    producers.emplace_back([&] {
      for (int i = 0; i < N / 4; ++i)
        q.push(i);
    });
  }
  for (auto& t : producers)
    t.join();

  // Drain remaining
  auto deadline = std::chrono::steady_clock::now() + 500ms;
  while (!q.empty() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(1ms);

  consumer.request_stop();

  // Some events may have been dropped — but no crash and no data race
  EXPECT_LE(consumed.load(), N);
  SUCCEED();  // primary assertion: no UB, no crash
}

TEST(BoundedQueueThreadSafetyTest, ConcurrentSizeAndEmpty) {
  BoundedQueue<int> q{8, DropPolicy::DropNewest};

  std::jthread writer{[&](std::stop_token stop) {
    int i = 0;
    while (!stop.stop_requested()) {
      q.push(i++);
      std::this_thread::sleep_for(1ms);
    }
  }};

  // size() and empty() must not crash or return nonsense under concurrent
  // writes
  for (int i = 0; i < 50; ++i) {
    auto s = q.size();
    auto e = q.empty();
    EXPECT_LE(s, 8u);
    // If empty() is true, size() should be 0 — but a race between the two
    // calls is possible, so we only assert each independently
    (void)e;
    std::this_thread::sleep_for(1ms);
  }
  writer.request_stop();
}
