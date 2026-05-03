#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <thread>

#include "CommonTypes.h"
#include "IPublish.h"

class Publisher {
 public:
  explicit Publisher(std::shared_ptr<IPublish> broker);

  bool publish(Event&& event);
  bool publishTimes(Event event, uint8_t maxRetries);

 private:
  std::shared_ptr<IPublish> _broker;
};