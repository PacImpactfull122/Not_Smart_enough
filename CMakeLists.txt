cmake_minimum_required(VERSION 3.25)
project(opencode_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(opencode_core INTERFACE)
target_include_directories(opencode_core INTERFACE src)

find_package(GTest QUIET)

if(GTest_FOUND)
  enable_testing()
  add_executable(test_arena tests/test_arena.cpp)
  target_link_libraries(test_arena PRIVATE opencode_core GTest::gtest Threads::Threads)
  add_test(NAME arena COMMAND test_arena)
endif()
