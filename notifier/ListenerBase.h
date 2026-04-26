#pragma once

#include "Listener.h"

// ─────────────────────────────────────────────────────────────────────────────
// Concrete consumer Base
//
// Owns a Listener via shared_ptr. The callback captures [this] raw — safe
// because m_listener is a member: it cannot outlive the Base that owns it.
// Base stores m_notifier to allow move construction with re-subscription.
//
// Copy is deleted — copying would share one Listener between two Base
// instances, causing both callbacks to fire on the original Base's this.
//
// Move re-subscribes the new Base with a fresh Listener so the callback
// always points at the correct this. The old Listener is dropped and
// auto-detaches via its destructor.
// ─────────────────────────────────────────────────────────────────────────────

template <typename Derived>
class ListenerBase {
 public:
  explicit ListenerBase(NotifierPtr_t notifier) : m_notifier{notifier} {
    subscribe();
  }

  ListenerBase(const ListenerBase&) = delete;
  ListenerBase& operator=(const ListenerBase&) = delete;

  // Between reset() and subscribe() completing, this is a live object with no
  // active listener. If a notification arrives in that window from another
  // thread, it is silently missed.
  ListenerBase(ListenerBase&& other) : m_notifier{std::move(other.m_notifier)} {
    other.m_listener.reset();  // old listener detaches from Notifier
    subscribe();               // new listener with correct this
  }
  ListenerBase& operator=(ListenerBase&& other) {
    if (this != &other) {
      m_notifier = std::move(other.m_notifier);
      other.m_listener.reset();
      subscribe();
    }
    return *this;
  }

 protected:
  ~ListenerBase() = default;  // non-virtual — not a polymorphic base

 private:
  void subscribe() {
    static_assert(
        requires(Derived& d, Notifier::State s) { d.onStateUpdate(s); },
        "Derived must implement onStateUpdate(Notifier::State)");

    m_listener = Listener::create(m_notifier, [this](Notifier::State state) {
      static_cast<Derived*>(this)->onStateUpdate(state);
    });
  }

  NotifierPtr_t m_notifier;
  std::shared_ptr<Listener> m_listener;
};