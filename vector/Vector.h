#pragma once

#include <compare>
#include <format>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

template <typename T>
class Vector {
 public:
  Vector() : Vector(0) {}

  ~Vector() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      // Construct uninitialized spare slots so delete[] is safe
      try {
        std::uninitialized_default_construct(_data.get() + _size,
                                             _data.get() + _capacity);
      } catch (...) {
        std::terminate();
      }
    }
  }

  Vector(std::input_iterator auto beginIt, std::input_iterator auto endIt)
      : _size{static_cast<std::size_t>(std::distance(beginIt, endIt))},
        _capacity{_size},
        _data{std::make_unique_for_overwrite<T[]>(_size)},
        _view{std::span<T>(_data.get(), _size)} {
    std::uninitialized_copy(beginIt, endIt, _view.begin());
  }

  Vector(std::initializer_list<T> list)
      : _size{list.size()},
        _capacity{_size},
        _data{std::make_unique_for_overwrite<T[]>(list.size())},
        _view{std::span<T>(_data.get(), list.size())} {
    std::uninitialized_copy_n(list.begin(), _size, _view.begin());
  }

  explicit Vector(std::size_t size)
      : _size{size},
        _capacity{_size},
        _data{std::make_unique_for_overwrite<T[]>(size)},
        _view{std::span<T>(_data.get(), size)} {}

  Vector(std::size_t size, const T& val)
      : _size{size},
        _capacity{_size},
        _data{std::make_unique_for_overwrite<T[]>(size)},
        _view{std::span<T>(_data.get(), size)} {
    std::uninitialized_fill_n(_view.begin(), _size, val);
  }

  Vector(const Vector& other)
      : _size{other._size},
        _capacity{other._capacity},
        _data{std::make_unique_for_overwrite<T[]>(other._size)},
        _view{std::span<T>(_data.get(), other._size)} {
    std::uninitialized_copy_n(other.begin(), _size, _view.begin());
  }

  const Vector& operator=(const Vector& other) {
    if (&other == this) {
      return *this;
    }

    _size = other._size;
    _capacity = other._capacity;
    auto tmp = std::make_unique_for_overwrite<T[]>(other._size);
    _data.swap(tmp);
    _view = std::span<T>(_data.get(), other._size);

    std::uninitialized_copy_n(other.begin(), _size, _view.begin());

    return *this;
  }

  Vector(Vector&& other) noexcept
      : _size{other._size},
        _capacity{other._capacity},
        _data{std::move(other._data)},
        _view{std::span<T>(_data.get(), other._size)} {
    other._size = 0;
    other._capacity = 0;
  }

  const Vector& operator=(Vector&& other) noexcept {
    if (&other == this) {
      return *this;
    }

    _size = other._size;
    _capacity = other._capacity;
    std::exchange(_data, std::move(other._data));
    _view = std::span<T>(_data.get(), other._size);

    other._size = 0;
    other._capacity = 0;

    return *this;
  }

  bool operator==(const Vector& other) const {
    return std::equal(this->begin(), this->end(), other.begin(), other.end());
  }

  auto operator<=>(const Vector& other) const
    requires std::three_way_comparable<T>
  {
    // Lexicographic compare over elements
    return std::lexicographical_compare_three_way(
        _view.begin(), _view.end(), other._view.begin(), other._view.end());
  }

  const T& at(std::size_t pos) const {
    if (pos >= _view.size()) {
      throw std::out_of_range("Index out of bounds");
    }

    return _view[pos];
  }

  const T& operator[](std::size_t pos) const { return _view[pos]; }
  T& operator[](std::size_t pos) { return _view[pos]; }

  std::size_t size() const { return _size; }

  std::size_t capacity() const { return _capacity; }

  bool empty() const { return _view.empty(); }

  void push_back(const T& item) {
    detail::checkAllocation(*this, _size + 1);

    // insert data
    _data[_size++] = item;

    // update view
    _view = std::span<T>(_data.get(), _size);
  }
  void push_back(T&& item) {
    detail::checkAllocation(*this, _size + 1);

    _data[_size++] = std::move(item);
    _view = std::span<T>(_data.get(), _size);
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    detail::checkAllocation(*this, _size + 1);

    std::construct_at(&_data[_size++], std::forward<Args>(args)...);
    _view = std::span<T>(_data.get(), _size);
  }

  void insert(std::input_iterator auto inputIt, const T& val) {
    if (inputIt == this->end()) {
      this->push_back(val);
      return;
    }

    // Save offset before potential reallocation
    const auto offset = std::distance(this->begin(), inputIt);
    this->push_back(T{});

    // Reconstruct iterator after potential realloc
    inputIt = this->begin() + offset;

    for (auto it = std::reverse_iterator(this->begin() + _size);
         it != std::reverse_iterator(inputIt); ++it) {
      *it = std::move(*std::next(it));
    }

    *inputIt = val;
  }
  void insert(std::contiguous_iterator auto posIt,
              std::input_iterator auto beginIt,
              std::input_iterator auto endIt) {
    const auto dist = std::distance(beginIt, endIt);
    const auto offset = std::distance(this->begin(), posIt);

    detail::checkAllocation(*this, _size + dist);
    posIt = this->begin() + offset;

    // shift existing elements after posIt forward by dist
    for (auto it = std::reverse_iterator(this->begin() + _size);
         it != std::reverse_iterator(posIt); ++it) {
      *std::prev(it, dist) = std::move(*it);
    }

    // insert new elements
    for (auto it = beginIt; it != endIt; ++it, ++posIt) {
      *posIt = *it;
    }

    _size += dist;
    _view = std::span<T>(_data.get(), _size);
  }

  void pop_back() {
    this->back().~T();
    --_size;
    _view = std::span<T>(_data.get(), _size);
  }

  void erase(std::input_iterator auto it) {
    for (; it != this->end() - 1; ++it) {
      it->~T();
      *it = std::move(*std::next(it));
    }

    --_size;
    _view = std::span<T>(_data.get(), _size);
  }
  void erase(std::input_iterator auto beginIt, std::input_iterator auto endIt) {
    const auto dist = std::distance(beginIt, endIt);
    for (auto it1 = beginIt, it2 = endIt; it1 != this->end() - dist;
         ++it1, ++it2) {
      it1->~T();
      *it1 = std::move(*it2);
    }

    _size -= dist;
    _view = std::span<T>(_data.get(), _size);
  }

  void reserve(std::size_t cap) {
    if ((cap < _capacity) or (cap == 0)) {
      return;
    }

    detail::checkAllocation(*this, cap);
  }

  void resize(std::size_t count) {
    if (count == _size) {
      return;
    }

    _size = count;
    if (count > _size) {
      detail::checkAllocation(*this, count);
    }

    _view = std::span<T>(_data.get(), count);
  }

  void clear() {
    std::destroy(_view.begin(), _view.end());

    _size = 0;
    _view = std::span<T>(_data.get(), _size);
  }

  const T& back() const { return _data[_size - 1]; }
  const T& front() const { return _data[0]; }

  T* data() { return _data.get(); }
  const T* data() const { return _data.get(); }

  std::contiguous_iterator auto begin() const { return _view.begin(); }
  std::contiguous_iterator auto end() const { return _view.end(); }
  std::contiguous_iterator auto const cbegin() const { return _view.begin(); }
  std::contiguous_iterator auto const cend() const { return _view.end(); }

  auto rbegin() { return std::reverse_iterator(_view.end()); }
  auto rend() { return std::reverse_iterator(_view.begin()); }

 private:
  std::size_t _size = 0;
  std::size_t _capacity = 0;
  std::unique_ptr<T[]> _data = nullptr;
  std::span<T> _view;

  struct detail {
    static void realloc(Vector& self, std::size_t cap) {
      try {
        auto tmp = std::make_unique_for_overwrite<T[]>(cap);
        // Move existing live elements into [0, _size)
        std::uninitialized_move(self._view.begin(), self._view.end(),
                                tmp.get());
        // tmp is safe to invoke delete[] as it was full already
        self._data.swap(tmp);
      } catch (const std::bad_alloc& e) {
        std::cerr << std::format("Allocation failed: {}\n", e.what());
        throw;
      }

      self._capacity = cap;
      self._view = std::span<T>(self._data.get(), self._size);
    }

    static void checkAllocation(Vector& self, std::size_t reqSize) {
      if (reqSize > self._capacity) {
        // the smallest power of 2 that is greater than or equal to required
        self._capacity = std::max(SSO_SIZE, std::bit_ceil(reqSize));
        detail::realloc(self, self._capacity);
      }
    }

   private:
    static constexpr std::size_t SSO_SIZE = 8;
  };
};