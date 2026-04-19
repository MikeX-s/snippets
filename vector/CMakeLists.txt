cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

project(Vector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type specified -- defaulting to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ── GoogleTest via FetchContent ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

# ── Google Benchmark ──────────────────────────────────────────────────────────
FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.4.zip
)
set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_library(${PROJECT_NAME}
    INTERFACE
    Vector.h
)

add_executable(Benchmark Benchmark.cpp)

target_include_directories(Benchmark PRIVATE ${CMAKE_SOURCE_DIR})

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

find_package(Threads REQUIRED)
target_link_libraries(Tests
    PRIVATE
        ${PROJECT_NAME}
        GTest::gtest_main
        Threads::Threads
)

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
  target_compile_options(Tests PRIVATE -fsanitize=thread -g)
  target_link_options   (Tests PRIVATE -fsanitize=thread)
endif()

option(ENABLE_VALGRIND "Enable Valgrind memory leak checks" OFF)
if(ENABLE_VALGRIND)
    find_program(VALGRIND_EXECUTABLE valgrind REQUIRED)
    add_test(NAME ValgrindTests
             COMMAND ${VALGRIND_EXECUTABLE}
                     --tool=memcheck
                     --leak-check=full
                     --error-exitcode=1
                     $<TARGET_FILE:Tests>)
endif()

if(ENABLE_TSAN AND ENABLE_VALGRIND)
    message(FATAL_ERROR "ENABLE_TSAN and ENABLE_VALGRIND are mutually exclusive")
endif()

include(GoogleTest)
gtest_discover_tests(Tests)