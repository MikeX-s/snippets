#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "CommonTypes.h"
#include "SubscriptionId.h"

// Subscription is a movable, non-copyable RAII handle. It holds — a
// weak_ptr<Broker> rather than a raw pointer or shared_ptr. This prevents
// Subscription from keeping the broker alive past its intended lifetime, and
// the destructor handles the case where the broker was already destroyed.

class Subscription {
 public:
  Subscription() = delete;

  explicit Subscription(std::weak_ptr<Broker> broker);

  ~Subscription() noexcept;

  Subscription(const Subscription& other) = delete;
  Subscription& operator=(const Subscription& other) = delete;

  Subscription(Subscription&& other) noexcept;
  Subscription& operator=(Subscription&& other) noexcept;

  bool operator==(const Subscription& other) const noexcept;

  SubscriptionId_t getId() const;

 private:
  std::optional<SubscriptionId_t> _id;
  std::weak_ptr<Broker> _broker;
};