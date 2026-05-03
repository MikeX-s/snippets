cmake_minimum_required(VERSION 4.3.0)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(cpp26 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_MODULE_STD ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(modulesCpp26 src/main.cpp)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)

set_target_properties(modulesCpp26 PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

target_compile_options(modulesCpp26 PRIVATE
    -freflection
    -Wall
    -Wextra
    -Wpedantic
    -Wno-dev)

target_link_libraries(modulesCpp26 PRIVATE pthread)
