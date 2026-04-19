
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "LruCache.h"  // include your implementation here

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────

template <typename KeyT, typename ValT>
void expectHit(LruCache<KeyT, ValT>& cache,
               const KeyT& key,
               const ValT& expected) {
  auto result = cache.get(key);
  ASSERT_TRUE(result.has_value()) << "Expected cache hit for key, got miss";
  EXPECT_EQ(*result, expected);
}

template <typename KeyT, typename ValT>
void expectMiss(LruCache<KeyT, ValT>& cache, const KeyT& key) {
  EXPECT_FALSE(cache.get(key).has_value()) << "Expected cache miss, got hit";
}

// ─────────────────────────────────────────────
// Basic behaviour
// ─────────────────────────────────────────────

TEST(LruCacheBasic, GetOnEmptyCacheReturnsMiss) {
  LruCache<int, int> cache(4);
  expectMiss(cache, 1);
}

TEST(LruCacheBasic, PutAndGetSingleEntry) {
  LruCache<int, int> cache(4);
  cache.put(1, 100);
  expectHit(cache, 1, 100);
}

TEST(LruCacheBasic, GetMissingKeyReturnNullopt) {
  LruCache<int, int> cache(4);
  cache.put(1, 10);
  EXPECT_EQ(cache.get(99), std::nullopt);
}

TEST(LruCacheBasic, PutOverwritesExistingKey) {
  LruCache<int, int> cache(4);
  cache.put(1, 10);
  cache.put(1, 20);
  expectHit(cache, 1, 20);
}

TEST(LruCacheBasic, MultiplePutAndGet) {
  LruCache<int, int> cache(4);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  expectHit(cache, 1, 10);
  expectHit(cache, 2, 20);
  expectHit(cache, 3, 30);
}

// ─────────────────────────────────────────────
// Capacity = 1  (edge case)
// ─────────────────────────────────────────────

TEST(LruCacheCapacityOne, SecondPutEvictsFirst) {
  LruCache<int, int> cache(1);
  cache.put(1, 10);
  cache.put(2, 20);
  expectMiss(cache, 1);
  expectHit(cache, 2, 20);
}

TEST(LruCacheCapacityOne, OverwriteSameKeyDoesNotEvict) {
  LruCache<int, int> cache(1);
  cache.put(1, 10);
  cache.put(1, 99);
  expectHit(cache, 1, 99);
}

// ─────────────────────────────────────────────
// Eviction order
// ─────────────────────────────────────────────

TEST(LruCacheEviction, EvictsLeastRecentlyUsed) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  // Key 1 is LRU — inserting key 4 should evict it
  cache.put(4, 40);
  expectMiss(cache, 1);
  expectHit(cache, 2, 20);
  expectHit(cache, 3, 30);
  expectHit(cache, 4, 40);
}

TEST(LruCacheEviction, GetRefreshesRecencyOrder) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  // Access key 1 — it is now MRU; key 2 becomes LRU
  cache.get(1);
  cache.put(4, 40);  // should evict key 2
  expectHit(cache, 1, 10);
  expectMiss(cache, 2);
  expectHit(cache, 3, 30);
  expectHit(cache, 4, 40);
}

TEST(LruCacheEviction, PutExistingKeyRefreshesOrder) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  // Re-putting key 1 with a new value makes it MRU; key 2 becomes LRU
  cache.put(1, 11);
  cache.put(4, 40);  // should evict key 2
  expectHit(cache, 1, 11);
  expectMiss(cache, 2);
  expectHit(cache, 3, 30);
  expectHit(cache, 4, 40);
}

TEST(LruCacheEviction, EvictionChain) {
  // Fill, then insert one new key per step, verifying each eviction in order
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  cache.put(4, 40);  // evicts 1
  expectMiss(cache, 1);

  cache.put(5, 50);  // evicts 2
  expectMiss(cache, 2);

  cache.put(6, 60);  // evicts 3
  expectMiss(cache, 3);

  expectHit(cache, 4, 40);
  expectHit(cache, 5, 50);
  expectHit(cache, 6, 60);
}

TEST(LruCacheEviction, AccessingAllKeepsNoneEvicted) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  // Touch every key before inserting — key 1 is MRU after this sequence
  cache.get(1);
  cache.get(2);
  cache.get(3);
  cache.get(1);      // order is now 2-3-1 (1=MRU), so 2 is LRU
  cache.put(4, 40);  // evicts 2
  expectMiss(cache, 2);
  expectHit(cache, 1, 10);
  expectHit(cache, 3, 30);
  expectHit(cache, 4, 40);
}

// ─────────────────────────────────────────────
// Capacity boundary — fill exactly to capacity
// ─────────────────────────────────────────────

TEST(LruCacheCapacity, FillToCapacityNoEviction) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  expectHit(cache, 1, 10);
  expectHit(cache, 2, 20);
  expectHit(cache, 3, 30);
}

TEST(LruCacheCapacity, OneOverCapacityEvictsExactlyOne) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  cache.put(4, 40);
  // Exactly one entry must be gone
  int misses = 0;
  for (int k : {1, 2, 3, 4})
    if (!cache.get(k).has_value())
      ++misses;
  EXPECT_EQ(misses, 1);
}

// ─────────────────────────────────────────────
// String keys and values
// ─────────────────────────────────────────────

TEST(LruCacheStringTypes, PutAndGetStringKeyValue) {
  LruCache<std::string, std::string> cache(4);
  cache.put("alpha", "AAA");
  cache.put("beta", "BBB");
  expectHit(cache, std::string("alpha"), std::string("AAA"));
  expectHit(cache, std::string("beta"), std::string("BBB"));
  expectMiss(cache, std::string("gamma"));
}

TEST(LruCacheStringTypes, EvictionWithStringKeys) {
  LruCache<std::string, int> cache(2);
  cache.put("x", 1);
  cache.put("y", 2);
  cache.put("z", 3);  // evicts "x"
  expectMiss(cache, std::string("x"));
  expectHit(cache, std::string("y"), 2);
  expectHit(cache, std::string("z"), 3);
}

// ─────────────────────────────────────────────
// Large capacity stress
// ─────────────────────────────────────────────

TEST(LruCacheStress, LargeSequentialInsert) {
  const int N = 1000;
  LruCache<int, int> cache(N);
  for (int i = 0; i < N; ++i)
    cache.put(i, i * 2);
  for (int i = 0; i < N; ++i)
    expectHit(cache, i, i * 2);
}

TEST(LruCacheStress, LargeEvictionSequence) {
  const int cap = 100;
  const int total = 500;
  LruCache<int, int> cache(cap);
  for (int i = 0; i < total; ++i)
    cache.put(i, i);
  // Only the last `cap` keys should survive
  for (int i = 0; i < total - cap; ++i)
    expectMiss(cache, i);
  for (int i = total - cap; i < total; ++i)
    expectHit(cache, i, i);
}

TEST(LruCacheStress, RepeatedGetDoesNotCorruptCache) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);
  for (int i = 0; i < 1000; ++i)
    expectHit(cache, 1 + (i % 3), 10 + (i % 3) * 10);
}