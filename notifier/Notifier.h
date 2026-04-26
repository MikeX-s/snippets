#pragma once

#include "IObserver.h"
#include "Utils.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <vector>

class Notifier;

using NotifierPtr_t = std::shared_ptr<Notifier>;

class Notifier {
 public:
  enum class State {
    Action1,
    Action2  //
  };

  using TemplateObserver = IObserver<State>;
  using ObserverPtr_t = std::weak_ptr<TemplateObserver>;

  Result attach(ObserverPtr_t observer) {
    if (observer.expired()) {
      return Result::NULLPTR;
    }

    {
      std::unique_lock lock(mtx);
      if (!m_observers.contains(observer)) {
        m_observers.insert(observer);
        return Result::OK;
      }
    }

    Log::logError("Multiple registrations not allowed");
    return Result::ERROR;
  }

  Result detach(ObserverPtr_t observer) {
    // Expired pointer can be detached, no check here

    {
      std::unique_lock lock(mtx);
      if (m_observers.contains(observer)) {
        m_observers.erase(observer);
        return Result::OK;
      }
    }

    Log::logError("No such observer found");
    return Result::NOT_FOUND;
  }

  void tryDetach(ObserverPtr_t observer) {
    std::unique_lock lock(mtx);
    if (m_observers.contains(observer)) {
      m_observers.erase(observer);
    }
  }

  void notify(State state) {
    std::vector<ObserverPtr_t> snapshot;
    {
      std::shared_lock lock(mtx);
      snapshot = {m_observers.begin(), m_observers.end()};
    }

    for (auto& observer : snapshot) {
      if (auto ptr = observer.lock()) {
        ptr->update(state);
      }
    }

    {
      // schedule removal of the expired
      std::unique_lock lock(mtx);
      std::erase_if(m_observers,
                    [](const ObserverPtr_t& o) { return o.expired(); });
    }
  }

 private:
  std::shared_mutex mtx;
  std::set<ObserverPtr_t, std::owner_less<ObserverPtr_t>> m_observers{};
};
