
#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "Vector.h"

// ─────────────────────────────────────────────────────────────────────────────
// TEST FIXTURE
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
class VectorTest : public ::testing::Test {};

using TestTypes = ::testing::Types<int, double, std::string>;
TYPED_TEST_SUITE(VectorTest, TestTypes);

// ═════════════════════════════════════════════════════════════════════════════
// 1. CONSTRUCTION
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorConstruction, DefaultConstructorProducesEmptyVector) {
  Vector<int> v;
  EXPECT_EQ(v.size(), 0u);
  EXPECT_EQ(v.capacity(), 0u);
  EXPECT_TRUE(v.empty());
}

TEST(VectorConstruction, FillConstructorSetsCorrectSizeAndValues) {
  Vector<int> v(5, 42);
  EXPECT_EQ(v.size(), 5u);
  for (std::size_t i = 0; i < v.size(); ++i)
    EXPECT_EQ(v[i], 42);
}

TEST(VectorConstruction, FillConstructorWithZeroCountProducesEmptyVector) {
  Vector<int> v(0, 99);
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
}

TEST(VectorConstruction, InitializerListConstructorPreservesOrder) {
  Vector<int> v = {10, 20, 30, 40, 50};
  ASSERT_EQ(v.size(), 5u);
  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v[2], 30);
  EXPECT_EQ(v[4], 50);
}

TEST(VectorConstruction, CopyConstructorProducesIndependentDeepCopy) {
  Vector<int> original = {1, 2, 3};
  Vector<int> copy(original);
  ASSERT_EQ(copy.size(), original.size());
  copy[0] = 999;
  EXPECT_EQ(original[0], 1);  // original must be unaffected
}

TEST(VectorConstruction, MoveConstructorTransfersOwnership) {
  Vector<int> source = {7, 8, 9};
  Vector<int> dest(std::move(source));
  EXPECT_EQ(dest.size(), 3u);
  EXPECT_EQ(dest[0], 7);
  EXPECT_EQ(source.size(), 0u);  // moved-from must be valid & empty
}

TEST(VectorConstruction, RangeConstructorCopiesIteratorRange) {
  int arr[] = {5, 6, 7, 8};
  Vector<int> v(std::begin(arr), std::end(arr));
  ASSERT_EQ(v.size(), 4u);
  EXPECT_EQ(v[0], 5);
  EXPECT_EQ(v[3], 8);
}

// ═════════════════════════════════════════════════════════════════════════════
// 2. ASSIGNMENT
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorAssignment, CopyAssignmentReplacesContentsWithDeepCopy) {
  Vector<int> a = {1, 2, 3};
  Vector<int> b = {9, 8};
  b = a;
  ASSERT_EQ(b.size(), 3u);
  EXPECT_EQ(b[2], 3);
  a[0] = 100;
  EXPECT_EQ(b[0], 1);  // b must not alias a
}

TEST(VectorAssignment, CopyAssignmentSelfAssignmentIsSafe) {
  Vector<int> v = {1, 2, 3};
  v = v;  // must not crash or corrupt
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[1], 2);
}

TEST(VectorAssignment, MoveAssignmentTransfersAndLeavesSourceEmpty) {
  Vector<int> src = {4, 5, 6};
  Vector<int> dst;
  dst = std::move(src);
  EXPECT_EQ(dst.size(), 3u);
  EXPECT_EQ(src.size(), 0u);
}

TEST(VectorAssignment, InitializerListAssignmentReplacesContents) {
  Vector<int> v = {1, 2, 3};
  v = {10, 20};
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v[1], 20);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3. ELEMENT ACCESS
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorElementAccess, SubscriptOperatorReadsAndWritesElements) {
  Vector<int> v = {1, 2, 3};
  v[1] = 99;
  EXPECT_EQ(v[1], 99);
}

TEST(VectorElementAccess, AtThrowsOutOfRangeForInvalidIndex) {
  Vector<int> v = {10, 20, 30};
  EXPECT_THROW(v.at(3), std::out_of_range);
  EXPECT_THROW(v.at(100), std::out_of_range);
}

TEST(VectorElementAccess, AtReturnsCorrectElementForValidIndex) {
  Vector<int> v = {10, 20, 30};
  EXPECT_EQ(v.at(0), 10);
  EXPECT_EQ(v.at(2), 30);
}

TEST(VectorElementAccess, FrontReturnsFirstElement) {
  Vector<int> v = {5, 6, 7};
  EXPECT_EQ(v.front(), 5);
}

TEST(VectorElementAccess, BackReturnsLastElement) {
  Vector<int> v = {5, 6, 7};
  EXPECT_EQ(v.back(), 7);
}

TEST(VectorElementAccess, DataReturnsPointerToUnderlyingArray) {
  Vector<int> v = {1, 2, 3};
  int* ptr = v.data();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr[0], 1);
  ptr[0] = 42;
  EXPECT_EQ(v[0], 42);
}

TEST(VectorElementAccess, ConstSubscriptOperatorReadsCorrectly) {
  const Vector<int> v = {3, 1, 4};
  EXPECT_EQ(v[0], 3);
  EXPECT_EQ(v[2], 4);
}

TEST(VectorElementAccess, ConstAtThrowsOutOfRangeForInvalidIndex) {
  const Vector<int> v = {1, 2};
  EXPECT_THROW(v.at(5), std::out_of_range);
}

// ═════════════════════════════════════════════════════════════════════════════
// 4. CAPACITY
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorCapacity, SizeReflectsNumberOfStoredElements) {
  Vector<int> v;
  EXPECT_EQ(v.size(), 0u);
  v.push_back(1);
  EXPECT_EQ(v.size(), 1u);
}

TEST(VectorCapacity, EmptyReturnsTrueOnlyWhenSizeIsZero) {
  Vector<int> v;
  EXPECT_TRUE(v.empty());
  v.push_back(1);
  EXPECT_FALSE(v.empty());
}

TEST(VectorCapacity, CapacityIsAtLeastSize) {
  Vector<int> v;
  for (int i = 0; i < 10; ++i)
    v.push_back(i);
  EXPECT_GE(v.capacity(), v.size());
}

TEST(VectorCapacity, ReserveIncreasesCapacityWithoutChangingSize) {
  Vector<int> v;
  v.reserve(100);
  EXPECT_GE(v.capacity(), 100u);
  EXPECT_EQ(v.size(), 0u);
}

TEST(VectorCapacity, ReserveDoesNotShrinkExistingCapacity) {
  Vector<int> v;
  v.reserve(50);
  std::size_t cap = v.capacity();
  v.reserve(10);
  EXPECT_GE(v.capacity(), cap);
}

TEST(VectorCapacity, CapacityDoublesOnGrowthByDefault) {
  Vector<int> v;
  v.push_back(0);
  std::size_t prev = v.capacity();
  while (v.size() < prev + 1)
    v.push_back(0);  // force reallocation
  EXPECT_GE(v.capacity(), prev * 2);
}

// ═════════════════════════════════════════════════════════════════════════════
// 5. MODIFIERS — push_back / pop_back / emplace_back
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorModifiers, PushBackAppendsCopyToEnd) {
  Vector<int> v;
  v.push_back(10);
  v.push_back(20);
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[1], 20);
}

TEST(VectorModifiers, PushBackMovesRvalueIntoVector) {
  Vector<std::string> v;
  std::string s = "hello";
  v.push_back(std::move(s));
  EXPECT_EQ(v[0], "hello");
  EXPECT_TRUE(s.empty());  // moved-from string must be in valid (empty) state
}

TEST(VectorModifiers, PopBackDecreasesSizeByOne) {
  Vector<int> v = {1, 2, 3};
  v.pop_back();
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v.back(), 2);
}

TEST(VectorModifiers, EmplaceBackConstructsElementInPlace) {
  Vector<std::pair<int, int>> v;
  v.emplace_back(3, 4);
  ASSERT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0].first, 3);
  EXPECT_EQ(v[0].second, 4);
}

TEST(VectorModifiers, PushBackManyElementsPreservesOrder) {
  Vector<int> v;
  for (int i = 0; i < 1000; ++i)
    v.push_back(i);
  EXPECT_EQ(v.size(), 1000u);
  for (int i = 0; i < 1000; ++i)
    ASSERT_EQ(v[i], i);
}

// ═════════════════════════════════════════════════════════════════════════════
// 6. MODIFIERS — insert
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorInsert, InsertAtBeginningShiftsElementsRight) {
  Vector<int> v = {2, 3, 4};
  v.insert(v.begin(), 1);
  ASSERT_EQ(v.size(), 4u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
}

TEST(VectorInsert, InsertAtEndEquivalentToPushBack) {
  Vector<int> v = {1, 2};
  v.insert(v.end(), 3);
  EXPECT_EQ(v.back(), 3);
  EXPECT_EQ(v.size(), 3u);
}

TEST(VectorInsert, InsertInMiddleShiftsElementsCorrectly) {
  Vector<int> v = {1, 3, 4};
  v.insert(v.begin() + 1, 2);
  ASSERT_EQ(v.size(), 4u);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(VectorInsert, InsertRangeExpandsVectorCorrectly) {
  Vector<int> v = {1, 5};
  int extra[] = {2, 3, 4};
  v.insert(v.begin() + 1, std::begin(extra), std::end(extra));
  ASSERT_EQ(v.size(), 5u);
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(v[i], i + 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// 7. MODIFIERS — erase
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorErase, EraseSingleElementAtBeginningShiftsLeft) {
  Vector<int> v = {1, 2, 3, 4};
  v.erase(v.begin());
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 2);
}

TEST(VectorErase, EraseSingleElementInMiddlePreservesNeighbours) {
  Vector<int> v = {10, 20, 30, 40};
  v.erase(v.begin() + 1);
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v[1], 30);
}

TEST(VectorErase, EraseRangeRemovesCorrectElements) {
  Vector<int> v = {1, 2, 3, 4, 5};
  v.erase(v.begin() + 1, v.begin() + 4);
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 5);
}

TEST(VectorErase, EraseLastElementMakesVectorEmpty) {
  Vector<int> v = {42};
  v.erase(v.begin());
  EXPECT_TRUE(v.empty());
}

// ═════════════════════════════════════════════════════════════════════════════
// 8. MODIFIERS — clear / resize / assign / swap
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorModifiers, ClearSetsZeroSizeButRetainsCapacity) {
  Vector<int> v = {1, 2, 3};
  std::size_t cap = v.capacity();
  v.clear();
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
  EXPECT_GE(v.capacity(), cap);
}

TEST(VectorModifiers, ResizeGrowsAndFillsWithDefaultValue) {
  Vector<int> v = {1, 2};
  v.resize(5);
  ASSERT_EQ(v.size(), 5u);
  EXPECT_EQ(v[2], 0);
  EXPECT_EQ(v[4], 0);
}

TEST(VectorModifiers, ResizeShrinksTruncatesElements) {
  Vector<int> v = {1, 2, 3, 4, 5};
  v.resize(3);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[2], 3);
}

// ═════════════════════════════════════════════════════════════════════════════
// 9. ITERATORS
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorIterators, BeginAndEndSpanAllElements) {
  Vector<int> v = {10, 20, 30};
  int sum = 0;
  for (auto it = v.begin(); it != v.end(); ++it)
    sum += *it;
  EXPECT_EQ(sum, 60);
}

TEST(VectorIterators, RangeBasedForLoopTraversesAllElements) {
  Vector<int> v = {1, 2, 3, 4};
  int product = 1;
  for (int x : v)
    product *= x;
  EXPECT_EQ(product, 24);
}

TEST(VectorIterators, ConstIteratorWorksOnConstVector) {
  const Vector<int> v = {5, 10, 15};
  int sum = 0;
  for (auto it = v.cbegin(); it != v.cend(); ++it)
    sum += *it;
  EXPECT_EQ(sum, 30);
}

TEST(VectorIterators, ReverseIteratorTraversesInReverseOrder) {
  Vector<int> v = {1, 2, 3};
  std::vector<int> reversed;
  for (auto it = v.rbegin(); it != v.rend(); ++it)
    reversed.push_back(*it);
  EXPECT_EQ(reversed[0], 3);
  EXPECT_EQ(reversed[2], 1);
}

TEST(VectorIterators, IteratorArithmeticWorksCorrectly) {
  Vector<int> v = {10, 20, 30, 40};
  auto it = v.begin();
  it += 2;
  EXPECT_EQ(*it, 30);
  EXPECT_EQ(*(it - 1), 20);
  EXPECT_EQ(it - v.begin(), 2);
}

TEST(VectorIterators, StdAlgorithmSortWorksViaIterators) {
  Vector<int> v = {5, 3, 1, 4, 2};
  std::sort(v.begin(), v.end());
  for (std::size_t i = 0; i < v.size(); ++i)
    EXPECT_EQ(v[i], static_cast<int>(i + 1));
}

TEST(VectorIterators, EmptyVectorBeginEqualsEnd) {
  Vector<int> v;
  EXPECT_EQ(v.begin(), v.end());
}

// ═════════════════════════════════════════════════════════════════════════════
// 10. COMPARISON OPERATORS
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorComparison, EqualityReturnsTrueForIdenticalVectors) {
  Vector<int> a = {1, 2, 3};
  Vector<int> b = {1, 2, 3};
  EXPECT_TRUE(a == b);
}

TEST(VectorComparison, EqualityReturnsFalseForDifferentContents) {
  Vector<int> a = {1, 2, 3};
  Vector<int> b = {1, 2, 4};
  EXPECT_FALSE(a == b);
}

TEST(VectorComparison, EqualityReturnsFalseForDifferentSizes) {
  Vector<int> a = {1, 2};
  Vector<int> b = {1, 2, 3};
  EXPECT_FALSE(a == b);
}

TEST(VectorComparison, InequalityOperatorIsConsistentWithEquality) {
  Vector<int> a = {1, 2};
  Vector<int> b = {1, 2};
  EXPECT_FALSE(a != b);
  b.push_back(3);
  EXPECT_TRUE(a != b);
}

TEST(VectorComparison, LessThanUsesLexicographicOrdering) {
  Vector<int> a = {1, 2, 3};
  Vector<int> b = {1, 2, 4};
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

TEST(VectorComparison, ShorterVectorIsLessThanLongerWithSamePrefix) {
  Vector<int> a = {1, 2};
  Vector<int> b = {1, 2, 0};
  EXPECT_TRUE(a < b);
}

// ═════════════════════════════════════════════════════════════════════════════
// 11. REALLOCATION SAFETY (object lifetime)
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorReallocation, ElementsRemainValidAfterReallocation) {
  Vector<int> v;
  v.reserve(2);
  v.push_back(1);
  v.push_back(2);
  // Force reallocation by exceeding capacity
  v.push_back(3);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(VectorReallocation, StringVectorSurvivesReallocationWithCorrectValues) {
  Vector<std::string> v;
  for (int i = 0; i < 100; ++i)
    v.push_back(std::to_string(i));
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(v[i], std::to_string(i));
}

// ═════════════════════════════════════════════════════════════════════════════
// 12. EDGE CASES
// ═════════════════════════════════════════════════════════════════════════════

TEST(VectorEdgeCases, PopBackOnSingleElementLeavesVectorEmpty) {
  Vector<int> v = {99};
  v.pop_back();
  EXPECT_TRUE(v.empty());
}

TEST(VectorEdgeCases, ResizeToSameSizeIsNoOp) {
  Vector<int> v = {1, 2, 3};
  v.resize(3);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 1);
}

TEST(VectorEdgeCases, ResizeToZeroEquivalentToClear) {
  Vector<int> v = {1, 2, 3};
  v.resize(0);
  EXPECT_TRUE(v.empty());
}

TEST(VectorEdgeCases, LargeReserveDoesNotCorruptData) {
  Vector<int> v = {1, 2, 3};
  v.reserve(10000);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[2], 3);
}

TEST(VectorEdgeCases, ClearFollowedByPushBackWorks) {
  Vector<int> v = {1, 2, 3};
  v.clear();
  v.push_back(42);
  EXPECT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0], 42);
}

TEST(VectorEdgeCases, CopyOfEmptyVectorIsEmpty) {
  Vector<int> a;
  Vector<int> b(a);
  EXPECT_TRUE(b.empty());
}

TEST(VectorEdgeCases, MoveOfEmptyVectorProducesEmptyVector) {
  Vector<int> a;
  Vector<int> b(std::move(a));
  EXPECT_TRUE(b.empty());
  EXPECT_TRUE(a.empty());
}

TEST(VectorEdgeCases, AtOnEmptyVectorAlwaysThrows) {
  Vector<int> v;
  EXPECT_THROW(v.at(0), std::out_of_range);
}

TEST(VectorEdgeCases, EraseFromEndIteratorRangeIsNoOp) {
  Vector<int> v = {1, 2, 3};
  v.erase(v.end(), v.end());
  EXPECT_EQ(v.size(), 3u);
}

TEST(VectorEdgeCases, InsertIntoEmptyVectorWorkLikePushBack) {
  Vector<int> v;
  v.insert(v.begin(), 77);
  ASSERT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0], 77);
}

// ═════════════════════════════════════════════════════════════════════════════
// 13. NON-TRIVIAL TYPES (destructor / copy semantics)
// ═════════════════════════════════════════════════════════════════════════════

namespace {
struct LifetimeTracker {
  static int live_count;
  LifetimeTracker() { ++live_count; }
  LifetimeTracker(const LifetimeTracker&) { ++live_count; }
  ~LifetimeTracker() { --live_count; }
};
int LifetimeTracker::live_count = 0;
}  // namespace

TEST(VectorLifetime, DestructorCallsDestructorOnAllElements) {
  LifetimeTracker::live_count = 0;
  {
    Vector<LifetimeTracker> v(5);
    EXPECT_EQ(LifetimeTracker::live_count, 5);
  }
  EXPECT_EQ(LifetimeTracker::live_count, 0);
}

TEST(VectorLifetime, ClearCallsDestructorOnAllElements) {
  LifetimeTracker::live_count = 0;
  Vector<LifetimeTracker> v(3);
  v.clear();
  EXPECT_EQ(LifetimeTracker::live_count, 0);
}

TEST(VectorLifetime, PopBackCallsDestructorOnRemovedElement) {
  LifetimeTracker::live_count = 0;
  Vector<LifetimeTracker> v(3);
  v.pop_back();
  EXPECT_EQ(LifetimeTracker::live_count, 2);
}

TEST(VectorLifetime, EraseCallsDestructorOnRemovedElements) {
  LifetimeTracker::live_count = 0;
  Vector<LifetimeTracker> v(4);
  v.erase(v.begin(), v.begin() + 2);
  EXPECT_EQ(LifetimeTracker::live_count, 2);
}

// ═════════════════════════════════════════════════════════════════════════════
// 14. TYPED TESTS — cross-type API smoke tests
// ═════════════════════════════════════════════════════════════════════════════

TYPED_TEST(VectorTest, DefaultConstructedVectorIsEmpty) {
  Vector<TypeParam> v;
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.size(), 0u);
}

TYPED_TEST(VectorTest, PushBackAndAccessWorkForAnyType) {
  Vector<TypeParam> v;
  v.push_back(TypeParam{});
  EXPECT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0], TypeParam{});
}

TYPED_TEST(VectorTest, ClearLeavesVectorEmptyForAnyType) {
  Vector<TypeParam> v;
  for (int i = 0; i < 5; ++i)
    v.push_back(TypeParam{});
  v.clear();
  EXPECT_TRUE(v.empty());
}
