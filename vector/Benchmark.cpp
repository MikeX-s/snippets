#include <benchmark/benchmark.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "Vector.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Prevent the compiler from optimising away a value.
template <typename T>
inline void do_not_optimise(T& v) {
  benchmark::DoNotOptimize(v);
}

// Build a pre-populated source vector of N integers.
static std::vector<int> make_source(int n) {
  std::vector<int> src(n);
  std::iota(src.begin(), src.end(), 0);
  return src;
}

// A non-trivial type: heap-allocated string payload.
struct HeavyString {
  std::string data;
  explicit HeavyString(int v = 0) : data(std::to_string(v) + "_payload") {}
  bool operator==(const HeavyString& o) const { return data == o.data; }
};

// ═════════════════════════════════════════════════════════════════════════════
// 1.  PUSH_BACK  (no pre-allocation)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_PushBack_Int(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    for (int i = 0; i < n; ++i)
      v.push_back(i);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_PushBack_Int, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/PushBack_Int");
BENCHMARK_TEMPLATE(BM_PushBack_Int, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/PushBack_Int");

template <template <typename> class Vec>
static void BM_PushBack_String(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<std::string> v;
    for (int i = 0; i < n; ++i)
      v.push_back(std::to_string(i));
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_PushBack_String, Vector)
    ->Range(64, 1 << 14)
    ->Name("Custom/PushBack_String");
BENCHMARK_TEMPLATE(BM_PushBack_String, std::vector)
    ->Range(64, 1 << 14)
    ->Name("Std/PushBack_String");

// ═════════════════════════════════════════════════════════════════════════════
// 2.  EMPLACE_BACK  (no pre-allocation)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_EmplaceBack_Int(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    for (int i = 0; i < n; ++i)
      v.emplace_back(i);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_EmplaceBack_Int, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/EmplaceBack_Int");
BENCHMARK_TEMPLATE(BM_EmplaceBack_Int, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/EmplaceBack_Int");

template <template <typename> class Vec>
static void BM_EmplaceBack_HeavyString(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<HeavyString> v;
    for (int i = 0; i < n; ++i)
      v.emplace_back(i);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_EmplaceBack_HeavyString, Vector)
    ->Range(64, 1 << 14)
    ->Name("Custom/EmplaceBack_HeavyString");
BENCHMARK_TEMPLATE(BM_EmplaceBack_HeavyString, std::vector)
    ->Range(64, 1 << 14)
    ->Name("Std/EmplaceBack_HeavyString");

// ═════════════════════════════════════════════════════════════════════════════
// 3.  PUSH_BACK WITH PRE-RESERVATION  (isolates reallocation cost)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_PushBack_Reserved_Int(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i)
      v.push_back(i);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_PushBack_Reserved_Int, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/PushBack_Reserved_Int");
BENCHMARK_TEMPLATE(BM_PushBack_Reserved_Int, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/PushBack_Reserved_Int");

// ═════════════════════════════════════════════════════════════════════════════
// 4.  RESERVE  (allocation cost in isolation)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_Reserve(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    benchmark::DoNotOptimize(v);
    v.reserve(n);
    benchmark::ClobberMemory();
  }
}
BENCHMARK_TEMPLATE(BM_Reserve, Vector)
    ->Range(64, 1 << 20)
    ->Name("Custom/Reserve");
BENCHMARK_TEMPLATE(BM_Reserve, std::vector)
    ->Range(64, 1 << 20)
    ->Name("Std/Reserve");

// ═════════════════════════════════════════════════════════════════════════════
// 5.  RANDOM ACCESS  (sequential read, cache-friendly)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_RandomAccess_Sequential(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> v(n);
  std::iota(v.begin(), v.end(), 0);
  for (auto _ : state) {
    long long sum = 0;
    for (int i = 0; i < n; ++i)
      sum += v[i];
    do_not_optimise(sum);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_RandomAccess_Sequential, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/RandomAccess_Sequential");
BENCHMARK_TEMPLATE(BM_RandomAccess_Sequential, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/RandomAccess_Sequential");

// ═════════════════════════════════════════════════════════════════════════════
// 6.  ITERATION  (range-for vs manual index)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_Iteration_RangeFor(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> v(n);
  std::iota(v.begin(), v.end(), 0);
  for (auto _ : state) {
    long long sum = 0;
    for (int x : v)
      sum += x;
    do_not_optimise(sum);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_Iteration_RangeFor, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/Iteration_RangeFor");
BENCHMARK_TEMPLATE(BM_Iteration_RangeFor, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/Iteration_RangeFor");

template <template <typename> class Vec>
static void BM_Iteration_Iterator(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> v(n);
  std::iota(v.begin(), v.end(), 0);
  for (auto _ : state) {
    long long sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it)
      sum += *it;
    do_not_optimise(sum);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_Iteration_Iterator, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/Iteration_Iterator");
BENCHMARK_TEMPLATE(BM_Iteration_Iterator, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/Iteration_Iterator");

// ═════════════════════════════════════════════════════════════════════════════
// 7.  INSERT AT BEGINNING  (worst-case shift cost)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_InsertAtBegin(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i)
      v.insert(v.begin(), i);
    do_not_optimise(v);
  }
  // O(n²) — keep n modest
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_InsertAtBegin, Vector)
    ->Range(16, 1 << 10)
    ->Name("Custom/InsertAtBegin");
BENCHMARK_TEMPLATE(BM_InsertAtBegin, std::vector)
    ->Range(16, 1 << 10)
    ->Name("Std/InsertAtBegin");

// ═════════════════════════════════════════════════════════════════════════════
// 8.  INSERT AT END / MIDDLE  (typical splice scenarios)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_InsertAtMiddle(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v(n, 0);
    auto mid = v.begin() + n / 2;
    v.insert(mid, -1);
    do_not_optimise(v);
  }
}
BENCHMARK_TEMPLATE(BM_InsertAtMiddle, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/InsertAtMiddle");
BENCHMARK_TEMPLATE(BM_InsertAtMiddle, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/InsertAtMiddle");

// ═════════════════════════════════════════════════════════════════════════════
// 9.  ERASE  (single element at begin / middle / end)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_EraseAtBegin(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    state.ResumeTiming();
    v.erase(v.begin());
    do_not_optimise(v);
  }
}
BENCHMARK_TEMPLATE(BM_EraseAtBegin, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/EraseAtBegin");
BENCHMARK_TEMPLATE(BM_EraseAtBegin, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/EraseAtBegin");

template <template <typename> class Vec>
static void BM_EraseAtMiddle(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    state.ResumeTiming();
    v.erase(v.begin() + n / 2);
    do_not_optimise(v);
  }
}
BENCHMARK_TEMPLATE(BM_EraseAtMiddle, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/EraseAtMiddle");
BENCHMARK_TEMPLATE(BM_EraseAtMiddle, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/EraseAtMiddle");

template <template <typename> class Vec>
static void BM_EraseRangeHalf(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    state.ResumeTiming();
    v.erase(v.begin(), v.begin() + n / 2);
    do_not_optimise(v);
  }
}
BENCHMARK_TEMPLATE(BM_EraseRangeHalf, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/EraseRangeHalf");
BENCHMARK_TEMPLATE(BM_EraseRangeHalf, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/EraseRangeHalf");

// ═════════════════════════════════════════════════════════════════════════════
// 10. POP_BACK  (repeated removal from end)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_PopBack(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    state.ResumeTiming();
    while (!v.empty())
      v.pop_back();
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_PopBack, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/PopBack");
BENCHMARK_TEMPLATE(BM_PopBack, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/PopBack");

// ═════════════════════════════════════════════════════════════════════════════
// 11. COPY CONSTRUCTION
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_CopyConstruct_Int(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> src(n);
  std::iota(src.begin(), src.end(), 0);
  for (auto _ : state) {
    Vec<int> copy(src);
    do_not_optimise(copy);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_CopyConstruct_Int, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/CopyConstruct_Int");
BENCHMARK_TEMPLATE(BM_CopyConstruct_Int, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/CopyConstruct_Int");

template <template <typename> class Vec>
static void BM_CopyConstruct_String(benchmark::State& state) {
  const int n = state.range(0);
  Vec<std::string> src;
  src.reserve(n);
  for (int i = 0; i < n; ++i)
    src.emplace_back(std::to_string(i));
  for (auto _ : state) {
    Vec<std::string> copy(src);
    do_not_optimise(copy);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_CopyConstruct_String, Vector)
    ->Range(64, 1 << 14)
    ->Name("Custom/CopyConstruct_String");
BENCHMARK_TEMPLATE(BM_CopyConstruct_String, std::vector)
    ->Range(64, 1 << 14)
    ->Name("Std/CopyConstruct_String");

// ═════════════════════════════════════════════════════════════════════════════
// 12. MOVE CONSTRUCTION
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_MoveConstruct_Int(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> src(n);
    std::iota(src.begin(), src.end(), 0);
    state.ResumeTiming();
    Vec<int> dest(std::move(src));
    do_not_optimise(dest);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_MoveConstruct_Int, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/MoveConstruct_Int");
BENCHMARK_TEMPLATE(BM_MoveConstruct_Int, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/MoveConstruct_Int");

// ═════════════════════════════════════════════════════════════════════════════
// 13. COPY ASSIGNMENT
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_CopyAssign_Int(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> src(n);
  std::iota(src.begin(), src.end(), 0);
  Vec<int> dst;
  for (auto _ : state) {
    dst = src;
    do_not_optimise(dst);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_CopyAssign_Int, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/CopyAssign_Int");
BENCHMARK_TEMPLATE(BM_CopyAssign_Int, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/CopyAssign_Int");

// ═════════════════════════════════════════════════════════════════════════════
// 14. MOVE ASSIGNMENT
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_MoveAssign_Int(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> dst;
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> src(n);
    std::iota(src.begin(), src.end(), 0);
    state.ResumeTiming();
    dst = std::move(src);
    do_not_optimise(dst);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_MoveAssign_Int, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/MoveAssign_Int");
BENCHMARK_TEMPLATE(BM_MoveAssign_Int, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/MoveAssign_Int");

// ═════════════════════════════════════════════════════════════════════════════
// 15. CLEAR
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_Clear_Int(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    state.ResumeTiming();
    v.clear();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_Clear_Int, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/Clear_Int");
BENCHMARK_TEMPLATE(BM_Clear_Int, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/Clear_Int");

template <template <typename> class Vec>
static void BM_Clear_String(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<std::string> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i)
      v.emplace_back(std::to_string(i));
    state.ResumeTiming();
    v.clear();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_Clear_String, Vector)
    ->Range(64, 1 << 14)
    ->Name("Custom/Clear_String");
BENCHMARK_TEMPLATE(BM_Clear_String, std::vector)
    ->Range(64, 1 << 14)
    ->Name("Std/Clear_String");

// ═════════════════════════════════════════════════════════════════════════════
// 16. RESIZE
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_ResizeGrow(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v;
    v.resize(n);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_ResizeGrow, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/ResizeGrow");
BENCHMARK_TEMPLATE(BM_ResizeGrow, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/ResizeGrow");

template <template <typename> class Vec>
static void BM_ResizeShrink(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    state.ResumeTiming();
    v.resize(n / 2);
    do_not_optimise(v);
  }
}
BENCHMARK_TEMPLATE(BM_ResizeShrink, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/ResizeShrink");
BENCHMARK_TEMPLATE(BM_ResizeShrink, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/ResizeShrink");

// ═════════════════════════════════════════════════════════════════════════════
// 17. SWAP
// ═════════════════════════════════════════════════════════════════════════════

/* template <template <typename> class Vec>
static void BM_Swap(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> a(n, 1), b(n, 2);
  for (auto _ : state) {
    a.swap(b);
    benchmark::ClobberMemory();
  }
}
BENCHMARK_TEMPLATE(BM_Swap, Vector)->Range(64, 1 << 20)->Name("Custom/Swap");
BENCHMARK_TEMPLATE(BM_Swap, std::vector)->Range(64, 1 << 20)->Name("Std/Swap"); */

// ═════════════════════════════════════════════════════════════════════════════
// 18. SHRINK_TO_FIT
// ═════════════════════════════════════════════════════════════════════════════

/* template <template <typename> class Vec>
static void BM_ShrinkToFit(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v;
    v.reserve(n * 4);
    for (int i = 0; i < n; ++i)
      v.push_back(i);
    state.ResumeTiming();
    v.shrink_to_fit();
    benchmark::ClobberMemory();
  }
}
BENCHMARK_TEMPLATE(BM_ShrinkToFit, Vector)
    ->Range(64, 1 << 16)
    ->Name("Custom/ShrinkToFit");
BENCHMARK_TEMPLATE(BM_ShrinkToFit, std::vector)
    ->Range(64, 1 << 16)
    ->Name("Std/ShrinkToFit"); */

// ═════════════════════════════════════════════════════════════════════════════
// 19. STD::SORT VIA ITERATORS
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_StdSort(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    Vec<int> v(n);
    // intentionally reverse-sorted — worst case for many sort implementations
    std::iota(v.rbegin(), v.rend(), 0);
    state.ResumeTiming();
    std::sort(v.begin(), v.end());
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_StdSort, Vector)
    ->Range(256, 1 << 18)
    ->Name("Custom/StdSort");
BENCHMARK_TEMPLATE(BM_StdSort, std::vector)
    ->Range(256, 1 << 18)
    ->Name("Std/StdSort");

// ═════════════════════════════════════════════════════════════════════════════
// 20. FILL CONSTRUCTOR
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_FillConstruct(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    Vec<int> v(n, 42);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_FillConstruct, Vector)
    ->Range(64, 1 << 20)
    ->Name("Custom/FillConstruct");
BENCHMARK_TEMPLATE(BM_FillConstruct, std::vector)
    ->Range(64, 1 << 20)
    ->Name("Std/FillConstruct");

// ═════════════════════════════════════════════════════════════════════════════
// 21. REALLOCATION COUNT STRESS  (push to 2^20 without reserve)
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_GrowthStress(benchmark::State& state) {
  for (auto _ : state) {
    Vec<int> v;
    for (int i = 0; i < (1 << 20); ++i)
      v.push_back(i);
    do_not_optimise(v);
  }
  state.SetItemsProcessed(state.iterations() * (1 << 20));
}
BENCHMARK_TEMPLATE(BM_GrowthStress, Vector)
    ->Name("Custom/GrowthStress")
    ->Iterations(10);
BENCHMARK_TEMPLATE(BM_GrowthStress, std::vector)
    ->Name("Std/GrowthStress")
    ->Iterations(10);

// ═════════════════════════════════════════════════════════════════════════════
// 22. DATA POINTER ACCESS (tight numerical loop through data())
// ═════════════════════════════════════════════════════════════════════════════

template <template <typename> class Vec>
static void BM_DataPointerAccess(benchmark::State& state) {
  const int n = state.range(0);
  Vec<int> v(n);
  std::iota(v.begin(), v.end(), 0);
  for (auto _ : state) {
    const int* ptr = v.data();
    long long sum = 0;
    for (int i = 0; i < n; ++i)
      sum += ptr[i];
    do_not_optimise(sum);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(int));
}
BENCHMARK_TEMPLATE(BM_DataPointerAccess, Vector)
    ->Range(256, 1 << 20)
    ->Name("Custom/DataPointerAccess");
BENCHMARK_TEMPLATE(BM_DataPointerAccess, std::vector)
    ->Range(256, 1 << 20)
    ->Name("Std/DataPointerAccess");
