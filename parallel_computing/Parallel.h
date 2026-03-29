#pragma once

#include <algorithm>
#include <future>
#include <set>
#include <vector>

template <typename T, typename Iter, typename F>
auto LoopHelper(Iter beg, Iter end, F&& fun) {
  std::set<T> result;
  std::transform(beg, end, std::inserter(result, result.begin()),
                 std::forward<F>(fun));

  return result;
}

// ArgType: extract the first argument type from any callable signature.
template <typename F>
struct ArgType;

// Plain function pointer  R(*)(T)
template <typename R, typename T>
struct ArgType<R (*)(T)> {
  using type = T;
};

template <typename F>
// The unary + forces an implicit conversion lambda to fn-pointer, which is only
// defined for *captureless* lambdas. Stateful lambdas have no such conversion
// and produce a hard error.
using FnType = decltype(+std::declval<F>());

template <typename Iter,
          typename F,
          typename FnArg = typename ArgType<FnType<F>>::type,
          typename = std::enable_if_t<std::is_invocable_v<F, FnArg>>>
auto computeParallel(Iter first, Iter last, F&& fun) {
  using ResultT = std::invoke_result_t<F, FnArg>;

  std::vector<std::future<std::set<ResultT>>> results;
  if (first == last)
    return results;

  std::size_t chunkNo = std::thread::hardware_concurrency();
  std::size_t chunkSize = std::distance(first, last) / chunkNo;

  if (chunkSize == 0) {
    chunkNo = 1;
    chunkSize = std::distance(first, last);
  }

  results.reserve(chunkNo);
  auto itBeg = first;
  auto itMid = first;

  for (std::size_t i = 0; i < chunkNo; ++i) {
    if (i < chunkNo - 1) {
      std::advance(itMid, chunkSize);
    } else {
      itMid = last;
    }

    // LoopHelper is explicitly instantiated with ResultT so that the
    // compiler can form a plain function pointer for std::async.
    results.push_back(
        std::async(std::launch::async,
                   LoopHelper<ResultT, Iter, std::remove_reference_t<F>>, itBeg,
                   itMid, std::forward<F>(fun)));

    itBeg = itMid;
  }

  for (auto&& async : results) {
    async.wait();
  }

  return results;
}