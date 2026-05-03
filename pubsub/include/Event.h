#pragma once

#include <chrono>

#include "CommonTypes.h"

struct Event {
  Topic_t topic;
  Payload_t payload;
  std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

class EventBuilder {
 public:
  EventBuilder& topic(Topic_t topic) {
    if (topic.empty()) {
      throw std::runtime_error("EventBuilder: topic cannot be empty");
    }
    _event.topic = std::move(topic);

    return *this;
  }

  template <typename P>
  EventBuilder& payload(std::shared_ptr<const P> payload) {
    if (payload == nullptr) {
      throw std::runtime_error{"EventBuilder: Payload nullptr"};
    }
    _event.payload = payload;

    return *this;
  }

  Event build() {
    _event.timestamp = std::chrono::steady_clock::now();

    return std::move(_event);
  }

 private:
  Event _event;
};