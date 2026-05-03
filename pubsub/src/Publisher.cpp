#include "Publisher.h"
#include "Event.h"

Publisher::Publisher(std::shared_ptr<IPublish> broker) : _broker{broker} {}

bool Publisher::publish(Event&& event) {
  return _broker->publish(std::move(event)).has_value();
}

bool Publisher::publishTimes(Event event, uint8_t maxRetries) {
  static thread_local std::mt19937 rng{std::random_device{}()};

  std::chrono::milliseconds baseWait{1};
  const std::chrono::milliseconds maxWait{100};

  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    auto result = _broker->publish(std::move(event));

    if (result.has_value()) {
      return true;
    }

    event = std::move(result.error());

    if (attempt == maxRetries) {
      break;
    }

    // Backoff & Jitter
    auto range = baseWait.count() / 4;
    std::uniform_int_distribution<long long> dist(-range, range);
    std::this_thread::sleep_for(baseWait +
                                std::chrono::milliseconds(dist(rng)));

    baseWait = std::min(baseWait * 2, maxWait);
  }

  return false;
}