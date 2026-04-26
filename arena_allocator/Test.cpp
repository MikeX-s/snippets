#pragma GCC diagnostic ignored "-Wunused-result"

#include <gtest/gtest.h>

#include "ArenaAllocator.h"

#include <list>
#include <vector>

// ─── Test fixture
// ─────────────────────────────────────────────────────────────

template <std::size_t N>
struct ArenaFixtureN : public ::testing::Test {
  ArenaStorage<N> storage;
};

using SmallArena = ArenaFixtureN<256>;
using MediumArena = ArenaFixtureN<4096>;

// ─── 1. Basic allocation
// ──────────────────────────────────────────────────────

TEST_F(SmallArena, AllocateReturnsNonNull) {
  ArenaAllocator<int, 256> a{storage};
  int* p = a.allocate(1);
  ASSERT_NE(p, nullptr);
}

TEST_F(SmallArena, AllocateAdvancesOffset) {
  ArenaAllocator<int, 256> a{storage};
  const std::size_t before = storage.used();
  a.allocate(4);  // 4 ints = 16 bytes
  EXPECT_GT(storage.used(), before);
}

TEST_F(SmallArena, AllocateReturnsSufficientSpace) {
  ArenaAllocator<int, 256> a{storage};
  int* p = a.allocate(8);
  // Write and read back all 8 elements — would crash/ASAN-fail if too small
  for (int i = 0; i < 8; ++i)
    p[i] = i * 2;
  for (int i = 0; i < 8; ++i)
    EXPECT_EQ(p[i], i * 2);
}

TEST_F(SmallArena, AllocateThrowsWhenExhausted) {
  ArenaAllocator<std::byte, 256> a{storage};
  EXPECT_NO_THROW(a.allocate(256));  // exact fit
  EXPECT_THROW(a.allocate(1), std::bad_alloc);
}

TEST_F(SmallArena, AllocateThrowsOnOversizedSingleRequest) {
  ArenaAllocator<int, 256> a{storage};
  EXPECT_THROW(a.allocate(1000), std::bad_alloc);
}

TEST_F(SmallArena, AllocateZeroBytes) {
  // Allocating 0 elements should either succeed or throw — must not crash
  ArenaAllocator<int, 256> a{storage};
  EXPECT_NO_THROW({ [[maybe_unused]] int* p = a.allocate(0); });
}

// ─── 2. Alignment ────────────────────────────────────────────────────────────

TEST_F(MediumArena, IntAllocationsAreAligned) {
  ArenaAllocator<int, 4096> a{storage};
  for (int i = 0; i < 10; ++i) {
    int* p = a.allocate(1);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(int), 0u)
        << "Allocation " << i << " is misaligned";
  }
}

TEST_F(MediumArena, DoubleAllocationsAreAligned) {
  ArenaAllocator<double, 4096> a{storage};
  for (int i = 0; i < 8; ++i) {
    double* p = a.allocate(1);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(double), 0u);
  }
}

struct alignas(64) OverAligned {
  char data[64];
};

TEST_F(MediumArena, OverAlignedTypeRespectedByArena) {
  ArenaAllocator<OverAligned, 4096> a{storage};
  OverAligned* p = a.allocate(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64, 0u);
}

TEST_F(MediumArena, MixedTypeAllocationsAllAligned) {
  // Simulate what list/map do: rebind to node type
  ArenaAllocator<char, 4096> ca{storage};
  ArenaAllocator<double, 4096> da{ca};  // rebind constructor

  ca.allocate(1);  // disturb alignment
  double* p = da.allocate(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(double), 0u);
}

// ─── 3. Deallocate is a no-op
// ─────────────────────────────────────────────────

TEST_F(SmallArena, DeallocateDoesNotReduceUsed) {
  ArenaAllocator<int, 256> a{storage};
  int* p = a.allocate(4);
  const std::size_t used_after_alloc = storage.used();
  a.deallocate(p, 4);
  EXPECT_EQ(storage.used(), used_after_alloc);
}

TEST_F(SmallArena, DeallocateNullptrDoesNotCrash) {
  ArenaAllocator<int, 256> a{storage};
  EXPECT_NO_THROW(a.deallocate(nullptr, 0));
}

// ─── 4. ArenaStorage::reset
// ───────────────────────────────────────────────────

TEST_F(SmallArena, ResetReclainsAllMemory) {
  ArenaAllocator<int, 256> a{storage};
  a.allocate(32);
  EXPECT_GT(storage.used(), 0u);
  storage.reset();
  EXPECT_EQ(storage.used(), 0u);
  EXPECT_EQ(storage.free(), storage.capacity());
}

TEST_F(SmallArena, CanAllocateAgainAfterReset) {
  ArenaAllocator<int, 256> a{storage};
  a.allocate(64 / sizeof(int));
  storage.reset();
  EXPECT_NO_THROW(a.allocate(64 / sizeof(int)));
}

// ─── 5. allocator_traits dispatch ────────────────────────────────────────────

TEST_F(MediumArena, TraitsAllocateMatchesDirect) {
  using Alloc = ArenaAllocator<int, 4096>;
  using Traits = std::allocator_traits<Alloc>;
  Alloc a{storage};

  int* p1 = a.allocate(1);
  storage.reset();
  int* p2 = Traits::allocate(a, 1);

  // Both should return the base of the arena (after reset)
  EXPECT_EQ(p1, p2);
}

TEST_F(MediumArena, TraitsConstructAndDestroyRoundTrip) {
  using Alloc = ArenaAllocator<std::string, 4096>;
  using Traits = std::allocator_traits<Alloc>;
  Alloc a{storage};

  std::string* p = Traits::allocate(a, 1);
  Traits::construct(a, p, "hello arena");
  EXPECT_EQ(*p, "hello arena");
  Traits::destroy(a, p);  // destructor must run without crash
  Traits::deallocate(a, p, 1);
}

TEST_F(MediumArena, TraitsMaxSize) {
  using Alloc = ArenaAllocator<int, 4096>;
  using Traits = std::allocator_traits<Alloc>;
  Alloc a{storage};
  EXPECT_EQ(Traits::max_size(a), 4096u / sizeof(int));
}

// ─── 6. Equality / identity
// ───────────────────────────────────────────────────

TEST_F(SmallArena, SameStorageAllocatorsAreEqual) {
  ArenaAllocator<int, 256> a1{storage};
  ArenaAllocator<int, 256> a2{storage};
  EXPECT_TRUE(a1 == a2);
  EXPECT_FALSE(a1 != a2);
}

TEST_F(SmallArena, DifferentStorageAllocatorsAreUnequal) {
  ArenaStorage<256> other_storage;
  ArenaAllocator<int, 256> a1{storage};
  ArenaAllocator<int, 256> a2{other_storage};
  EXPECT_FALSE(a1 == a2);
  EXPECT_TRUE(a1 != a2);
}

TEST_F(SmallArena, ReoundTripRebindPreservesEquality) {
  ArenaAllocator<int, 256> a1{storage};
  ArenaAllocator<char, 256> a2{a1};  // rebind ctor
  ArenaAllocator<int, 256> a3{a2};   // rebind back
  EXPECT_TRUE(a1 == a3);
}

// ─── 7. Rebind
// ────────────────────────────────────────────────────────────────

TEST_F(MediumArena, RebindAllocatorSharesArena) {
  ArenaAllocator<int, 4096> int_alloc{storage};
  using CharAlloc = typename ArenaAllocator<int, 4096>::rebind<char>::other;
  CharAlloc char_alloc{int_alloc};

  EXPECT_EQ(int_alloc.storage(), char_alloc.storage());
}

TEST_F(MediumArena, RebindAllocationsUseSameBuffer) {
  ArenaAllocator<int, 4096> a{storage};
  using ByteAlloc = ArenaAllocator<std::byte, 4096>;
  ByteAlloc ba{a};

  int* pi = a.allocate(1);
  std::byte* pb = ba.allocate(1);
  // Both pointers must be within the same storage block
  const auto* base = reinterpret_cast<const std::byte*>(&storage);
  EXPECT_GE(reinterpret_cast<uintptr_t>(pi), reinterpret_cast<uintptr_t>(base));
  EXPECT_GE(reinterpret_cast<uintptr_t>(pb), reinterpret_cast<uintptr_t>(base));
}

// ─── 8. Propagation traits
// ────────────────────────────────────────────────────

TEST(ArenaPropagationTraits, POCMAIsTrue) {
  using Alloc = ArenaAllocator<int, 256>;
  static_assert(std::allocator_traits<
                    Alloc>::propagate_on_container_move_assignment::value,
                "POCMA must be true_type for O(1) container move");
}

TEST(ArenaPropagationTraits, POCCAIsFalse) {
  using Alloc = ArenaAllocator<int, 256>;
  static_assert(!std::allocator_traits<
                    Alloc>::propagate_on_container_copy_assignment::value,
                "POCCA must be false_type");
}

TEST(ArenaPropagationTraits, POCSIsTrue) {
  using Alloc = ArenaAllocator<int, 256>;
  static_assert(
      std::allocator_traits<Alloc>::propagate_on_container_swap::value,
      "POCS must be true_type");
}

TEST(ArenaPropagationTraits, IsAlwaysEqualIsFalse) {
  using Alloc = ArenaAllocator<int, 256>;
  static_assert(!std::allocator_traits<Alloc>::is_always_equal::value,
                "Stateful allocator must not claim is_always_equal");
}

// ─── 9. std::vector integration ──────────────────────────────────────────────

TEST_F(MediumArena, VectorPushBackNoHeap) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  std::vector<int, Alloc> v{a};

  const std::size_t before = storage.used();
  v.reserve(128);  // This will allocate 512 bytes

  for (int i = 0; i < 128; ++i)
    v.push_back(i);

  // All allocations came from the arena — heap usage is zero
  EXPECT_GT(storage.used(), before);
  for (int i = 0; i < 128; ++i)
    EXPECT_EQ(v[i], i);
}

TEST_F(MediumArena, VectorMoveTransfersAllocator) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  std::vector<int, Alloc> v1{a};
  v1.push_back(1);
  v1.push_back(2);

  std::vector<int, Alloc> v2{std::move(v1)};
  // POCMA = true → allocator is moved, no per-element copy
  ASSERT_EQ(v2.size(), 2u);
  EXPECT_EQ(v2[0], 1);
  EXPECT_EQ(v2[1], 2);
  // v1 is in a valid but unspecified state after move
}

TEST_F(MediumArena, VectorSelectOnCopyConstruction) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  std::vector<int, Alloc> v1{a};
  v1.push_back(42);

  // Copy ctor calls select_on_container_copy_construction
  std::vector<int, Alloc> v2{v1};
  ASSERT_EQ(v2.size(), 1u);
  EXPECT_EQ(v2[0], 42);
}

TEST_F(MediumArena, VectorReserveExhaustsArena) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  std::vector<int, Alloc> v{a};
  // Reserving more than the arena can hold should throw
  EXPECT_THROW(v.reserve(4096 / sizeof(int) + 1), std::length_error);
}

// ─── 10. std::list integration (exercises rebind) ────────────────────────────

TEST_F(MediumArena, ListPushBackUsesArena) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  std::list<int, Alloc> lst{a};

  for (int i = 0; i < 20; ++i)
    lst.push_back(i);
  ASSERT_EQ(lst.size(), 20u);

  int expected = 0;
  for (int val : lst)
    EXPECT_EQ(val, expected++);
}

TEST_F(MediumArena, ListNodeAllocationsShareArena) {
  using Alloc = ArenaAllocator<int, 4096>;
  Alloc a{storage};
  const std::size_t before = storage.used();
  {
    std::list<int, Alloc> lst{a};
    lst.push_back(1);
    lst.push_back(2);
    EXPECT_GT(storage.used(), before);
  }
  // Nodes destroyed but arena still holds the memory
  storage.reset();
  EXPECT_EQ(storage.used(), 0u);
}

// ─── 11. Construct / destroy with non-trivial types ──────────────────────────

struct LifetimeTracker {
  static int live_count;
  int value;
  explicit LifetimeTracker(int v) : value(v) { ++live_count; }
  ~LifetimeTracker() { --live_count; }
  LifetimeTracker(const LifetimeTracker& o) : value(o.value) { ++live_count; }
};
int LifetimeTracker::live_count = 0;

TEST_F(MediumArena, ConstructRunsConstructor) {
  using Alloc = ArenaAllocator<LifetimeTracker, 4096>;
  using Traits = std::allocator_traits<Alloc>;
  Alloc a{storage};

  LifetimeTracker::live_count = 0;
  LifetimeTracker* p = Traits::allocate(a, 1);
  Traits::construct(a, p, 99);

  EXPECT_EQ(LifetimeTracker::live_count, 1);
  EXPECT_EQ(p->value, 99);

  Traits::destroy(a, p);
  EXPECT_EQ(LifetimeTracker::live_count, 0);
  Traits::deallocate(a, p, 1);
}

TEST_F(MediumArena, VectorOfNonTrivialTypeDestroysElements) {
  using Alloc = ArenaAllocator<LifetimeTracker, 4096>;
  Alloc a{storage};

  LifetimeTracker::live_count = 0;
  {
    std::vector<LifetimeTracker, Alloc> v{a};
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    EXPECT_EQ(LifetimeTracker::live_count, 3);
  }  // vector destructor calls Traits::destroy on each element
  EXPECT_EQ(LifetimeTracker::live_count, 0);
}

// ─── 12. Multi-container sharing one arena ───────────────────────────────────

TEST_F(MediumArena, TwoVectorsShareArena) {
  ArenaAllocator<int, 4096> ai{storage};
  ArenaAllocator<double, 4096> ad{storage};

  std::vector<int, ArenaAllocator<int, 4096>> vi{ai};
  std::vector<double, ArenaAllocator<double, 4096>> vd{ad};

  vi.reserve(16);
  vd.reserve(16);

  for (int i = 0; i < 16; ++i)
    vi.push_back(i);
  for (double d = 0; d < 16; ++d)
    vd.push_back(d);

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(vi[i], i);
  for (int i = 0; i < 16; ++i)
    EXPECT_DOUBLE_EQ(vd[i], static_cast<double>(i));

  storage.reset();  // free both at once
  EXPECT_EQ(storage.used(), 0u);
}