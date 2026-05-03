#include "Subscription.h"
#include "Broker.h"

Subscription::Subscription(std::weak_ptr<Broker> broker)
    : _id{SubscriptionId::next()}, _broker{broker} {}

Subscription::~Subscription() noexcept {
  if (auto ptr = _broker.lock(); (ptr != nullptr) and _id.has_value() and
                                 _id.value() != SubscriptionId::invalid) {
    ptr->unsubscribe(_id.value());
  }
}

Subscription::Subscription(Subscription&& other) noexcept
    : _broker{std::move(other._broker)} {
  _id.swap(other._id);
  other._id.reset();
}
Subscription& Subscription::operator=(Subscription&& other) noexcept {
  if (this != &other) {
    _id.swap(other._id);
    _broker.swap(other._broker);
    other._id.reset();
  }

  return *this;
}

bool Subscription::operator==(const Subscription& other) const noexcept {
  if (!other._id.has_value()) {
    return false;
  }

  return this->_id == other._id;
}

SubscriptionId_t Subscription::getId() const {
  return _id.value_or(SubscriptionId::invalid);
}