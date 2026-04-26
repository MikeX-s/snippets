cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

project(ArenaAllocator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.zip
)
set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_library(${PROJECT_NAME}
    INTERFACE
    ArenaAllocator.h
)

add_executable(Benchmark Benchmark.cpp)

target_include_directories(Benchmark PRIVATE ${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)

target_link_libraries(Benchmark
    PRIVATE
    benchmark::benchmark_main
    Threads::Threads
)

enable_testing()

add_executable(Tests
    Test.cpp
)

target_include_directories(Tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(Tests
    PRIVATE
    ${PROJECT_NAME}
    GTest::gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(Tests)