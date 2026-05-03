#pragma once

#include "CommonTypes.h"

struct ISubscribe {
  virtual ~ISubscribe() = default;
  virtual Subscription subscribe(const Topic_t& topic, Callback_t callback) = 0;
};
