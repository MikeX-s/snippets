cmake_minimum_required(VERSION 4.3.0)

# -------------------------------------------------------
#  import std; gate — still required as of CMake 4.3.
#  UUID for CMake 4.3 — check cmake/Help/dev/experimental.rst
#  after a CMake upgrade if this stops working.
#  Must appear BEFORE project().
# -------------------------------------------------------
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(cpp26 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Modules require Ninja — fail fast rather than silently misbehaving
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "C++ modules require the Ninja generator. Use: cmake -G Ninja")
endif()

# -------------------------------------------------------
#  Reflection: GCC trunk only (-freflection).
#  Not available in mainline Clang 22 (Bloomberg clang-p2996
#  fork implements it but is not packaged in Fedora).
# -------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Compiler: GCC ${CMAKE_CXX_COMPILER_VERSION} — reflection enabled")
    set(REFLECTION_FLAGS "-freflection")
else()
    message(WARNING "Compiler is not GCC — -freflection not supported, reflection disabled")
    set(REFLECTION_FLAGS "")
endif()

add_executable(modulesCpp26 src/main.cpp)

target_compile_options(modulesCpp26 PRIVATE
    ${REFLECTION_FLAGS}
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(modulesCpp26 PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

target_link_libraries(modulesCpp26 PRIVATE pthread)