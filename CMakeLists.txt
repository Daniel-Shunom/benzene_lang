cmake_minimum_required(VERSION 3.16)
project(ether VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Executable output directory")

# Compiler processes can consume substantial memory. Ninja otherwise defaults
# to CPU-count parallelism, including when invoked without --parallel.
set(ETHER_BUILD_JOBS "1" CACHE STRING "Maximum simultaneous compile/link jobs")
if (NOT ETHER_BUILD_JOBS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "ETHER_BUILD_JOBS must be a positive integer")
endif()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS ether_build=${ETHER_BUILD_JOBS})
set(CMAKE_JOB_POOL_COMPILE ether_build)
set(CMAKE_JOB_POOL_LINK ether_build)

add_subdirectory(core)
add_subdirectory(cli)

include(cmake/Installation.cmake)

option(ETHER_BUILD_TESTS "Build the ether test suite" ON)
if (ETHER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
