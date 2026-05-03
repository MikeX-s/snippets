#pragma once

#include <memory>

#include "CommonTypes.h"
#include "ISubscribe.h"

struct LifetimeToken {};

class Subscriber {
 public:
  explicit Subscriber(std::shared_ptr<ISubscribe> broker);

  Subscription subscribe(const Topic_t& topic, Callback_t callback);

 private:
  const std::shared_ptr<LifetimeToken> _token =
      std::make_shared<LifetimeToken>();

  std::shared_ptr<ISubscribe> _broker;
};