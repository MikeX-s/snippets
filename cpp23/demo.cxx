module;

#include <cstdio>

export module demo;

import std;

export void fun(std::string str) {
    std::println("s= {}", str);
}