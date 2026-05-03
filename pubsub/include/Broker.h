#pragma once

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "BoundedQueue.h"
#include "Event.h"
#include "IPublish.h"
#include "ISubscribe.h"
#include "Subscription.h"
#include "Utils.h"

// subscribe() locks exclusively, inserts the callback, returns a Subscription.
// unsubscribe() locks exclusively, erases by ID.
// The dispatcher is a std::jthread member. It starts in the constructor body,
// after all members are fully initialised. It loops calling queue.pop(), then
// for each event locks the registry with a shared lock, copies the relevant
// callbacks into a local vector, releases the lock, and invokes each callback.

class Broker : public std::enable_shared_from_this<Broker>,
               public ISubscribe,
               public IPublish {
 private:
  struct Token {
    explicit Token() = default;
  };

 public:
  explicit Broker([[maybe_unused]] Token t);
  ~Broker() override;

  static std::shared_ptr<Broker> create();

  std::expected<void, Event> publish(Event&& event) override;
  Subscription subscribe(const Topic_t& topic, Callback_t cbk) override;

 private:
  void unsubscribe(SubscriptionId_t id);
  void loop(std::stop_token stop);
  void handleEvent(Event&& event);

  std::shared_mutex _mtx;
  std::unordered_map<Topic_t, std::unordered_map<SubscriptionId_t, Callback_t>>
      _registry{};
  std::unordered_map<SubscriptionId_t, Topic_t> _idToTopic;

  BoundedQueue<Event> _queue{64, DropPolicy::DropOldest};
  std::atomic<std::thread::id> _dispatchingThreadId{std::thread::id()};
  // The jthread must be the last member, so it is first to be destroyed.
  std::jthread _dispatcher;

  friend class Subscription;
};