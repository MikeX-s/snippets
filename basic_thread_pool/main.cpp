#include "ThreadPool.h"

#include <format>
#include <iostream>

using std::format;

int main() {
  ThreadPool pool;

  auto f1 = pool.submit([]() { std::cout << format("Hello there!\n"); });
  f1.get();

  std::function l1 = []() { std::cout << format("Hello there!\n"); };
  auto f2 = pool.submit(l1);
  f2.get();

  auto f3 = pool.submit([]() { return 6 * 7; });
  std::cout << format("6 * 7 = {}\n", f3.get());

  constexpr int N = 8;
  std::vector<std::future<long long>> chunks;
  chunks.reserve(N);

  for (int i = 1; i <= N; ++i) {
    chunks.push_back(pool.submit([i] {
      // Simulate work: sum 1..i*1000
      long long sum = 0;
      for (int j = 1; j <= i * 1000; ++j)
        sum += j;
      return sum;
    }));
  }

  long long total = 0;
  for (auto& f : chunks)
    total += f.get();

  std::cout << format("batch sum = {}\n", total);

  return 0;
}