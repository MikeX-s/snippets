#pragma once

#include "CommonTypes.h"

#include <expected>

struct IPublish {
  virtual ~IPublish() = default;
  virtual std::expected<void, Event> publish(Event&& event) = 0;
};
