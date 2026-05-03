#pragma once

#include <functional>
#include <memory>
#include <variant>

class Event;
class Broker;
class Subscription;

struct DataType1 {
  float value{0.f};
  uint32_t sensorId{0};
  bool operator==(const DataType1&) const = default;
};

using Callback_t = std::function<void(const Event&)>;
using Topic_t = std::string;
using SubscriptionId_t = uint64_t;

using Payload_t = std::variant<std::shared_ptr<const DataType1>,  //
                               std::monostate>;