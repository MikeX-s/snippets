#pragma once

template <typename State>
class IObserver {
public:
  virtual ~IObserver() = default;
  virtual void update(State state) = 0;
};