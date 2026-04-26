#pragma once

#include <memory>
#include <stdexcept>

template <typename std::size_t N>
class ArenaStorage {
 public:
  std::byte* allocate(std::size_t bytes, std::size_t align) {
    // Get the raw address of the current position
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(_buffer + _offset);

    // Calculate how much to add to raw_addr to reach a multiple of 'align'
    uintptr_t aligned_addr = (raw_addr + align - 1) & ~(align - 1);

    // Calculate the new offset relative to the beginning of the array
    std::size_t new_offset =
        (aligned_addr - reinterpret_cast<uintptr_t>(_buffer));

    if (new_offset + bytes > N)
      throw std::bad_alloc{};

    _offset = new_offset + bytes;

    return reinterpret_cast<std::byte*>(aligned_addr);
  }

  void reset() noexcept { _offset = 0; }
  std::size_t capacity() const noexcept { return N; }
  std::size_t free() const noexcept { return N - _offset; }
  std::size_t used() const noexcept { return _offset; }

 private:
  alignas(std::max_align_t) std::byte _buffer[N];
  std::size_t _offset = 0;
};

template <typename T, std::size_t N>
class ArenaAllocator {
 public:
  using value_type = T;

  // Move assignment carries the allocator (enables O(1) container move)
  using propagate_on_container_move_assignment = std::true_type;
  // Copy assignment does NOT propagate (each copied container keeps its own)
  using propagate_on_container_copy_assignment = std::false_type;
  // Swap propagates — both sides must agree or behaviour is undefined
  using propagate_on_container_swap = std::true_type;
  // Stateful: two instances pointing to different arenas are NOT equal
  using is_always_equal = std::false_type;

  explicit ArenaAllocator(ArenaStorage<N>& s) noexcept : _storage(&s) {}

  template <typename U>
  ArenaAllocator(const ArenaAllocator<U, N>& other) noexcept
      : _storage{other._storage} {}

  template <typename U>
  struct rebind {
    using other = ArenaAllocator<U, N>;
  };

  [[nodiscard]] T* allocate(std::size_t size) {
    return reinterpret_cast<T*>(
        _storage->allocate(size * sizeof(T), alignof(T)));
  }

  void deallocate(T* ptr, std::size_t size) noexcept {
    // no-op, memory freed, when arena goes out of scope
  }

  std::size_t max_size() const noexcept { return N / sizeof(T); }

  ArenaAllocator select_on_container_copy_construction() const noexcept {
    return *this;
  }

  template <typename U>
  bool operator==(const ArenaAllocator<U, N>& other) const noexcept {
    return _storage == other._storage;
  }

  template <typename U>
  bool operator!=(const ArenaAllocator<U, N>& other) const noexcept {
    return !(*this == other);
  }

 private:
  ArenaStorage<N>* _storage;

  FRIEND_TEST(MediumArena, RebindAllocatorSharesArena);
  ArenaStorage<N>* storage() const noexcept { return _storage; }

  template <typename U, std::size_t M>
  friend class ArenaAllocator;
};