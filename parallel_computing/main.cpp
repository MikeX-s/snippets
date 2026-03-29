#include "Parallel.h"

#include <array>
#include <bitset>
#include <iostream>
#include <random>

using Result_t = int;

int main() {
  std::vector<Result_t> input(512, 0);
  std::iota(input.begin(), input.end(), 0);

  auto lambda = [](const Result_t& val) { return val * val; };

  auto chunks = computeParallel(input.begin(), input.end(), lambda);

  std::vector<Result_t> finalResult;
  for (auto&& chunk : chunks)
    if (chunk.valid()) {
      auto s = chunk.get();
      finalResult.insert(finalResult.end(), s.begin(), s.end());
    }

  return 0;
}