cmake_minimum_required(VERSION 3.16)
project(basic_thread_pool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} main.cpp ThreadPool.cpp)