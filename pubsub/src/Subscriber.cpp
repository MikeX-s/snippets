#include "Subscriber.h"
#include "Broker.h"
#include "Subscription.h"

Subscriber::Subscriber(std::shared_ptr<ISubscribe> broker) : _broker{broker} {}

Subscription Subscriber::subscribe(const Topic_t& topic, Callback_t callback) {
  auto weakToken = std::weak_ptr<LifetimeToken>(_token);
  return _broker->subscribe(
      topic, [weakToken, cb = std::move(callback)](const Event& e) {
        if (weakToken.lock()) {
          std::invoke(cb, e);
        }
      });
}
