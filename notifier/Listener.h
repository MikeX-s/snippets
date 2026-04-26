#pragma once

#include "Notifier.h"

#include <format>
#include <functional>

class Listener final : public std::enable_shared_from_this<Listener>,
                       public Notifier::TemplateObserver {
 private:
  // External code cannot obtain a Token by any means, so the constructor of
  // Listener is effectively unreachable from outside —  without breaking
  // make_shared
  class Token {
    Token() = default;
    friend class Listener;
  };

 public:
  using Callback = std::function<void(Notifier::State)>;

  Listener([[maybe_unused]] Token, NotifierPtr_t notifier, Callback callback)
      : m_notifier{std::move(notifier)}, m_callback{std::move(callback)} {
    if (!m_notifier) {
      throw std::invalid_argument("Notifier can't be nullptr");
    }
    if (!m_callback) {
      throw std::invalid_argument("Callback can't be nullptr");
    }
  }

  ~Listener() noexcept override {
    try {
      m_notifier->tryDetach(this->getWeakPtr());
    } catch (...) {
      // destructor must not throw
    }
  }

  // Copied or moved Listener will call getWeakPtr() incorrectly —
  // weak_from_this() only works if the object is already managed by a
  // shared_ptr. A copy-constructed Listener not created via create() will have
  // a dangling observer registration situation.
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  Listener(Listener&&) = delete;
  Listener& operator=(Listener&&) = delete;

  static std::shared_ptr<Listener> create(NotifierPtr_t notifier,
                                          Callback callback) {
    auto temp =
        std::make_shared<Listener>(Token{}, notifier, std::move(callback));

    Result res = notifier->attach(temp->getWeakPtr());
    if (res != Result::OK) {
      Log::logError(
          std::format("Attach failed, status: {}", static_cast<int>(res)));
      return nullptr;
    }

    return temp;
  }

  void update(Notifier::State state) override {
    if (m_callback) {
      std::invoke(m_callback, state);
    }
  }

 private:
  [[nodiscard]] std::weak_ptr<Listener> getWeakPtr() {
    return weak_from_this();  // throws bad_weak_ptr on misuse — intentional
  }

  NotifierPtr_t m_notifier{nullptr};
  Callback m_callback;
};