#include "Broker.h"

Broker::Broker([[maybe_unused]] Token t) {
  _dispatcher = std::jthread{&Broker::loop, this};
}

Broker::~Broker() {
  _dispatcher.request_stop();
}

std::shared_ptr<Broker> Broker::create() {
  return std::make_shared<Broker>(Token{});
}

std::expected<void, Event> Broker::publish(Event&& event) {
  return _queue.push(std::move(event), _dispatcher.get_stop_token());
}

Subscription Broker::subscribe(const Topic_t& topic, Callback_t cbk) {
  std::unique_lock<std::shared_mutex> lock{_mtx};

  auto sub = Subscription(weak_from_this());
  _registry[topic].emplace(sub.getId(), std::move(cbk));
  _idToTopic.emplace(sub.getId(), topic);

  return sub;
}

void Broker::unsubscribe(SubscriptionId_t id) {
  if (std::this_thread::get_id() == _dispatchingThreadId.load()) {
    Log::logError(
        "Deadlock detected: Cannot unsubscribe from within a callback!");
    return;
  }

  {
    // Quickly modify the maps
    std::unique_lock<std::shared_mutex> lock{_mtx};

    auto it = _idToTopic.find(id);
    if (it == _idToTopic.end()) {
      Log::logError("Not found subscription");
      return;
    }

    auto topic = it->second;
    _idToTopic.erase(it);

    if (auto regIt = _registry.find(topic); regIt != _registry.end()) {
      regIt->second.erase(id);
    } else {
      Log::logError("No sync between data");
    }
  }

  // Synchronization barrier (Quiescent State)
  // Acquire the lock again. If the Dispatcher is still executing
  // this specific callback (because it was already in progress), this line will
  // wait for it to finish.
  std::unique_lock<std::shared_mutex> barrier{_mtx};

  // Once we reach this point, we are 100% certain that no calls
  // to the removed callback are still in progress.
}

void Broker::loop(std::stop_token stop) {
  Event event;

  while (_queue.pop(event, stop)) {
    handleEvent(std::move(event));
  }
}

void Broker::handleEvent(Event&& event) {
  std::vector<std::pair<SubscriptionId_t, Callback_t>> snapshot;

  std::shared_lock<std::shared_mutex> lock{_mtx};

  _dispatchingThreadId.store(std::this_thread::get_id());

  if (const auto it = _registry.find(event.topic); it != _registry.cend()) {
    const auto& [_, innerMap] = *it;
    snapshot.assign(innerMap.begin(), innerMap.end());
  } else {
    Log::logError("No subscribers found for topic: " + event.topic);
  }

  // The shared_lock ensures that a concurrent unsubscribe() call blocks
  // on the unique_lock barrier until all callbacks in this loop have completed.
  for (const auto& [_, cb] : snapshot) {
    try {
      std::invoke(cb, event);
    } catch (const std::exception& e) {
      Log::logError(e.what());
    } catch (...) {
      Log::logError("Unknown exception in callback");
    }
  }

  // clean after use
  _dispatchingThreadId.store(std::thread::id());
}
