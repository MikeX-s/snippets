#include "Listener.h"
#include "ListenerBase.h"
#include "Notifier.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <memory>
#include <thread>
#include <vector>

class Widget : public ListenerBase<Widget> {
 public:
  explicit Widget(NotifierPtr_t notifier) : ListenerBase(notifier) {}

  Widget(Widget&& other) noexcept
      : ListenerBase(std::move(other)),
        m_action1Count{other.m_action1Count.load()},
        m_action2Count{other.m_action2Count.load()} {}

  // Counters exposed for test assertions.
  int action1Count() const { return m_action1Count.load(); }
  int action2Count() const { return m_action2Count.load(); }

 private:
  void onStateUpdate(Notifier::State state) {
    switch (state) {
      case Notifier::State::Action1:
        ++m_action1Count;
        break;
      case Notifier::State::Action2:
        ++m_action2Count;
        break;
    }
  }

  std::atomic<int> m_action1Count{0};
  std::atomic<int> m_action2Count{0};

  friend class ListenerBase<Widget>;
};

// ─────────────────────────────────────────────────────────────────────────────
// MockObserver — low-level helper for testing Notifier directly,
// bypassing Listener and Widget entirely.
// ─────────────────────────────────────────────────────────────────────────────

class MockObserver : public Notifier::TemplateObserver {
 public:
  void update(Notifier::State state) override {
    ++callCount;
    lastState.store(state);
  }

  std::atomic<int> callCount{0};
  std::atomic<Notifier::State> lastState{Notifier::State::Action1};
};

struct MockHandle {
  std::shared_ptr<MockObserver> observer = std::make_shared<MockObserver>();
  std::weak_ptr<MockObserver> weak = observer;
};

// ─────────────────────────────────────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────────────────────────────────────

class NotifierTest : public ::testing::Test {
 protected:
  std::shared_ptr<Notifier> notifier = std::make_shared<Notifier>();
};

// ─────────────────────────────────────────────────────────────────────────────
// Suite 1 — attach / detach contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, Attach_ValidObserver_ReturnsOK) {
  MockHandle h;
  EXPECT_EQ(notifier->attach(h.weak), Result::OK);
}

TEST_F(NotifierTest, Attach_ExpiredObserver_ReturnsNullptr) {
  Notifier::ObserverPtr_t expired;
  EXPECT_EQ(notifier->attach(expired), Result::NULLPTR);
}

TEST_F(NotifierTest, Attach_SameObserverTwice_ReturnsError) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  EXPECT_EQ(notifier->attach(h.weak), Result::ERROR);
}

TEST_F(NotifierTest, Attach_SecondObserverAfterDuplicate_ReturnsOK) {
  MockHandle h1, h2;
  ASSERT_EQ(notifier->attach(h1.weak), Result::OK);
  ASSERT_EQ(notifier->attach(h1.weak), Result::ERROR);
  EXPECT_EQ(notifier->attach(h2.weak), Result::OK);
}

TEST_F(NotifierTest, Detach_AttachedObserver_ReturnsOK) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  EXPECT_EQ(notifier->detach(h.weak), Result::OK);
}

TEST_F(NotifierTest, Detach_UnknownObserver_ReturnsNotFound) {
  MockHandle h;
  EXPECT_NE(notifier->detach(h.weak), Result::OK);
}

TEST_F(NotifierTest, Detach_ExpiredPreviouslyAttached_ReturnsOK) {
  Notifier::ObserverPtr_t captured;
  {
    MockHandle h;
    ASSERT_EQ(notifier->attach(h.weak), Result::OK);
    captured = h.weak;
  }
  EXPECT_EQ(notifier->detach(captured), Result::OK);
}

TEST_F(NotifierTest, ReattachAfterDetach_ReturnsOK) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  ASSERT_EQ(notifier->detach(h.weak), Result::OK);
  EXPECT_EQ(notifier->attach(h.weak), Result::OK);
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 2 — notify() delivery via MockObserver
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, Notify_Action1_DeliveredToObserver) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(h.observer->callCount.load(), 1);
  EXPECT_EQ(h.observer->lastState.load(), Notifier::State::Action1);
}

TEST_F(NotifierTest, Notify_Action2_DeliveredToObserver) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  notifier->notify(Notifier::State::Action2);
  EXPECT_EQ(h.observer->callCount.load(), 1);
  EXPECT_EQ(h.observer->lastState.load(), Notifier::State::Action2);
}

TEST_F(NotifierTest, Notify_CalledTwice_ObserverReceivesBoth) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  notifier->notify(Notifier::State::Action2);
  EXPECT_EQ(h.observer->callCount.load(), 2);
  EXPECT_EQ(h.observer->lastState.load(), Notifier::State::Action2);
}

TEST_F(NotifierTest, Notify_NoObservers_DoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(notifier->notify(Notifier::State::Action1));
}

TEST_F(NotifierTest, Notify_MultipleObservers_AllReceive) {
  MockHandle h1, h2, h3;
  ASSERT_EQ(notifier->attach(h1.weak), Result::OK);
  ASSERT_EQ(notifier->attach(h2.weak), Result::OK);
  ASSERT_EQ(notifier->attach(h3.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(h1.observer->callCount.load(), 1);
  EXPECT_EQ(h2.observer->callCount.load(), 1);
  EXPECT_EQ(h3.observer->callCount.load(), 1);
}

TEST_F(NotifierTest, Notify_DetachedObserver_NotReached) {
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  ASSERT_EQ(notifier->detach(h.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(h.observer->callCount.load(), 0);
}

TEST_F(NotifierTest, Notify_MixedObservers_OnlyAttachedReceives) {
  MockHandle attached, detached;
  ASSERT_EQ(notifier->attach(attached.weak), Result::OK);
  ASSERT_EQ(notifier->attach(detached.weak), Result::OK);
  ASSERT_EQ(notifier->detach(detached.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(attached.observer->callCount.load(), 1);
  EXPECT_EQ(detached.observer->callCount.load(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 3 — expired observer / lazy cleanup
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, Notify_ExpiredObserver_DoesNotCrash) {
  {
    MockHandle h;
    ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  }
  EXPECT_NO_FATAL_FAILURE(notifier->notify(Notifier::State::Action1));
}

TEST_F(NotifierTest, Notify_ExpiredObserver_RemovedByCleanupPass) {
  {
    MockHandle h;
    ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  }
  notifier->notify(Notifier::State::Action1);

  MockHandle fresh;
  ASSERT_EQ(notifier->attach(fresh.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(fresh.observer->callCount.load(), 1);
}

TEST_F(NotifierTest, Notify_SomeExpiredSomeLive_LiveOnesStillReceive) {
  MockHandle alive;
  ASSERT_EQ(notifier->attach(alive.weak), Result::OK);
  {
    MockHandle dead;
    ASSERT_EQ(notifier->attach(dead.weak), Result::OK);
  }
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(alive.observer->callCount.load(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 4 — Listener factory and callback contract
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, ListenerCreate_ValidArgs_ReturnsNonNull) {
  auto l = Listener::create(notifier, [](Notifier::State) {});
  EXPECT_NE(l, nullptr);
}

TEST_F(NotifierTest, ListenerCreate_NullNotifier_Throws) {
  EXPECT_THROW(Listener::create(nullptr, [](Notifier::State) {}),
               std::invalid_argument);
}

TEST_F(NotifierTest, ListenerCreate_NullCallback_Throws) {
  // Null std::function must be rejected at construction.
  Listener::Callback nullCb;
  EXPECT_THROW(Listener::create(notifier, nullCb), std::invalid_argument);
}

TEST_F(NotifierTest, ListenerCreate_CallbackInvokedOnNotify) {
  std::atomic<int> count{0};
  Notifier::State last{Notifier::State::Action1};

  auto l = Listener::create(notifier, [&](Notifier::State s) {
    ++count;
    last = s;
  });
  ASSERT_NE(l, nullptr);

  notifier->notify(Notifier::State::Action2);

  EXPECT_EQ(count.load(), 1);
  EXPECT_EQ(last, Notifier::State::Action2);
}

TEST_F(NotifierTest, ListenerCreate_TwoListeners_BothCallbacksInvoked) {
  std::atomic<int> count1{0}, count2{0};

  auto l1 = Listener::create(notifier, [&](Notifier::State) { ++count1; });
  auto l2 = Listener::create(notifier, [&](Notifier::State) { ++count2; });
  ASSERT_NE(l1, nullptr);
  ASSERT_NE(l2, nullptr);

  notifier->notify(Notifier::State::Action1);

  EXPECT_EQ(count1.load(), 1);
  EXPECT_EQ(count2.load(), 1);
}

TEST_F(NotifierTest, ListenerDestruction_AutoDetaches_CallbackNotInvoked) {
  std::atomic<int> count{0};
  {
    auto l = Listener::create(notifier, [&](Notifier::State) { ++count; });
    ASSERT_NE(l, nullptr);
  }  // l destroyed — detach() called in destructor

  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(count.load(), 0);
}

TEST_F(NotifierTest, ListenerDestruction_OtherListenerUnaffected) {
  std::atomic<int> count{0};
  auto survivor = Listener::create(notifier, [&](Notifier::State) { ++count; });

  {
    auto dropped = Listener::create(notifier, [](Notifier::State) {});
    ASSERT_NE(dropped, nullptr);
  }

  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(count.load(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 5 — Widget as concrete consumer
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, Widget_ReceivesAction1) {
  Widget w(notifier);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(w.action1Count(), 1);
  EXPECT_EQ(w.action2Count(), 0);
}

TEST_F(NotifierTest, Widget_ReceivesAction2) {
  Widget w(notifier);
  notifier->notify(Notifier::State::Action2);
  EXPECT_EQ(w.action1Count(), 0);
  EXPECT_EQ(w.action2Count(), 1);
}

TEST_F(NotifierTest, Widget_ReceivesBothStates) {
  Widget w(notifier);
  notifier->notify(Notifier::State::Action1);
  notifier->notify(Notifier::State::Action2);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(w.action1Count(), 2);
  EXPECT_EQ(w.action2Count(), 1);
}

TEST_F(NotifierTest, Widget_Destruction_StopsDelivery) {
  // Widget going out of scope must detach its Listener.
  // A MockObserver attached after proves the Notifier is still functional.
  {
    Widget w(notifier);
    notifier->notify(Notifier::State::Action1);
    EXPECT_EQ(w.action1Count(), 1);
  }  // Widget destroyed — Listener detaches

  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);
  notifier->notify(Notifier::State::Action1);
  EXPECT_EQ(h.observer->callCount.load(), 1);
}

TEST_F(NotifierTest, Widget_MultipleWidgets_EachReceivesIndependently) {
  Widget w1(notifier);
  Widget w2(notifier);

  notifier->notify(Notifier::State::Action1);
  notifier->notify(Notifier::State::Action2);

  EXPECT_EQ(w1.action1Count(), 1);
  EXPECT_EQ(w1.action2Count(), 1);
  EXPECT_EQ(w2.action1Count(), 1);
  EXPECT_EQ(w2.action2Count(), 1);
}

TEST_F(NotifierTest, Widget_DestroyOne_OtherContinuesReceiving) {
  auto w1 = std::make_unique<Widget>(notifier);
  Widget w2(notifier);

  notifier->notify(Notifier::State::Action1);
  w1.reset();  // destroy first widget

  notifier->notify(Notifier::State::Action1);

  // w2 must have received both notifications
  EXPECT_EQ(w2.action1Count(), 2);
}

TEST_F(NotifierTest, Widget_Move_ResubscribesWithCorrectThis) {
  // After move construction, the new Widget must receive notifications
  // on its own this — not the original's. The original must not receive
  // anything after the move.
  Widget original(notifier);
  Widget moved(std::move(original));

  notifier->notify(Notifier::State::Action1);

  // Moved-from Widget has no listener — receives nothing.
  EXPECT_EQ(original.action1Count(), 0);
  // Moved-to Widget re-subscribed — receives the notification.
  EXPECT_EQ(moved.action1Count(), 1);
}

TEST_F(NotifierTest, Widget_NotifierOutlivesWidget_NoUseAfterFree) {
  {
    Widget w(notifier);
    notifier->notify(Notifier::State::Action1);
  }
  EXPECT_NO_FATAL_FAILURE(notifier->notify(Notifier::State::Action1));
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 6 — thread safety
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(NotifierTest, ConcurrentAttach_NoCrashOrDataRace) {
  constexpr int N = 32;
  std::vector<MockHandle> handles(N);
  std::vector<std::thread> threads;
  std::latch ready(N);
  threads.reserve(N);

  for (int i = 0; i < N; ++i) {
    threads.emplace_back([&, i] {
      ready.arrive_and_wait();
      notifier->attach(handles[i].weak);
    });
  }
  for (auto& t : threads)
    t.join();

  notifier->notify(Notifier::State::Action1);
  for (auto& h : handles)
    EXPECT_GE(h.observer->callCount.load(), 1);
}

TEST_F(NotifierTest, ConcurrentDetach_NoCrashOrDataRace) {
  constexpr int N = 32;
  std::vector<MockHandle> handles(N);
  for (auto& h : handles)
    ASSERT_EQ(notifier->attach(h.weak), Result::OK);

  std::vector<std::thread> threads;
  std::latch ready(N);
  threads.reserve(N);

  for (int i = 0; i < N; ++i) {
    threads.emplace_back([&, i] {
      ready.arrive_and_wait();
      notifier->detach(handles[i].weak);
    });
  }
  for (auto& t : threads)
    t.join();

  EXPECT_NO_FATAL_FAILURE(notifier->notify(Notifier::State::Action1));
}

TEST_F(NotifierTest, ConcurrentNotify_SharedLockAllowsParallelSnapshots) {
  constexpr int N = 64;
  MockHandle h;
  ASSERT_EQ(notifier->attach(h.weak), Result::OK);

  std::vector<std::thread> threads;
  std::latch ready(N);
  threads.reserve(N);

  for (int i = 0; i < N; ++i) {
    threads.emplace_back([&, i] {
      ready.arrive_and_wait();
      notifier->notify(i % 2 == 0 ? Notifier::State::Action1
                                  : Notifier::State::Action2);
    });
  }
  for (auto& t : threads)
    t.join();

  EXPECT_EQ(h.observer->callCount.load(), N);
}

TEST_F(NotifierTest, ConcurrentAttachDetachAndNotify_NoCrash) {
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;

  threads.emplace_back([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      notifier->notify(Notifier::State::Action1);
      notifier->notify(Notifier::State::Action2);
    }
  });

  constexpr int A = 8;
  for (int i = 0; i < A; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 100; ++j) {
        MockHandle h;
        notifier->attach(h.weak);
        std::this_thread::sleep_for(std::chrono::microseconds(5));
        notifier->detach(h.weak);
      }
    });
  }

  for (std::size_t i = 1; i < threads.size(); ++i)
    threads[i].join();

  stop.store(true, std::memory_order_relaxed);
  threads[0].join();
}

TEST_F(NotifierTest, ConcurrentNotify_ObserverExpiresMidDispatch_NoCrash) {
  auto observer = std::make_shared<MockObserver>();
  ASSERT_EQ(notifier->attach(std::weak_ptr<MockObserver>(observer)),
            Result::OK);

  std::thread killer([&observer] {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    observer.reset();
  });

  for (int i = 0; i < 1000; ++i)
    notifier->notify(i % 2 == 0 ? Notifier::State::Action1
                                : Notifier::State::Action2);

  killer.join();
  EXPECT_NO_FATAL_FAILURE(notifier->notify(Notifier::State::Action1));
}

TEST_F(NotifierTest, ConcurrentWidget_MultipleWidgets_NoCrash) {
  // Widgets created and destroyed concurrently while Notifier fires.
  // Tests callback safety under real Widget lifetime churn.
  std::atomic<bool> stop{false};

  std::thread notifyThread([&] {
    while (!stop.load(std::memory_order_relaxed))
      notifier->notify(Notifier::State::Action1);
  });

  constexpr int W = 8;
  std::vector<std::thread> widgetThreads;
  widgetThreads.reserve(W);

  for (int i = 0; i < W; ++i) {
    widgetThreads.emplace_back([&] {
      for (int j = 0; j < 50; ++j) {
        Widget w(notifier);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    });
  }

  for (auto& t : widgetThreads)
    t.join();
  stop.store(true, std::memory_order_relaxed);
  notifyThread.join();
}

TEST_F(NotifierTest, ConcurrentNotify_LockNotHeldDuringDispatch) {
  constexpr int N = 16;
  std::vector<MockHandle> handles(N);
  for (auto& h : handles)
    ASSERT_EQ(notifier->attach(h.weak), Result::OK);

  std::latch ready(N);
  std::vector<std::thread> threads;
  threads.reserve(N);

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < N; ++i) {
    threads.emplace_back([&] {
      ready.arrive_and_wait();
      notifier->notify(Notifier::State::Action1);
    });
  }
  for (auto& t : threads)
    t.join();
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_LT(elapsed, std::chrono::milliseconds(500));
}