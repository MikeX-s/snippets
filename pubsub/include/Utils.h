#pragma once

#include <iostream>
#include <string_view>
#include <syncstream>

struct Log {
  static inline void logError(std::string_view msg) {
    std::osyncstream(std::cerr) << msg << '\n';
  }
};

enum class Result : int8_t {
  OK = 1,
  ERROR = -1,
  NULLPTR = -2,
  NOT_FOUND = -3,
  UNINITIALIZED = -128  //
};