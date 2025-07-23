cmake_minimum_required(VERSION 4.0.0)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_COMPILER g++)

project(cpp23 VERSION 1.0.0 LANGUAGES CXX)

message(STATUS "CMake ${CMAKE_VERSION}")

# Create the target
add_executable(modulesCpp23 main.cpp)

# Set C++23 standard with proper target properties
target_compile_features(modulesCpp23 PRIVATE cxx_std_23)

# Let's nicely support folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Ensure the standard is required, not optional
set_target_properties(modulesCpp23 PROPERTIES
    CMAKE_CXX_STANDARD 23
    CMAKE_CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_compile_options(modulesCpp23 PRIVATE 
    -Wall 
    -Wextra
    -Wpedantic
    -Wno-dev)

add_library(demo)

target_compile_features(demo PRIVATE cxx_std_23)

target_sources(demo PUBLIC FILE_SET CXX_MODULES FILES demo.cxx)

target_link_libraries(modulesCpp23 PRIVATE demo)