#pragma once

#include <atomic>
#include <cstdint>

using SubscriptionId_t = uint64_t;

namespace SubscriptionId {

inline SubscriptionId_t next() {
  static std::atomic<SubscriptionId_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

constexpr SubscriptionId_t invalid = 0;

}  // namespace SubscriptionId