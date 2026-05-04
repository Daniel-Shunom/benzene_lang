cmake_minimum_required(VERSION 3.16)
project(ether LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(core)
add_subdirectory(cli)

option(ETHER_BUILD_TESTS "Build the ether test suite" ON)
if (ETHER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
