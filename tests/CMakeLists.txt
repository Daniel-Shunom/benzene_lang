include(FetchContent)

# doctest v2.4.11 calls cmake_minimum_required(VERSION ~2.8). Modern CMake
# refuses that without an explicit policy floor, so we set one here.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

set(ETHER_TEST_SOURCES
  unit/test_main.cpp
  unit/test_lexer.cpp
  unit/test_tables.cpp
  unit/test_diagnostic_engine.cpp
  unit/test_symbol_table.cpp
  unit/test_parser.cpp
  unit/test_sym_resolver.cpp
  unit/test_import_res.cpp
  integration/test_module_pipeline.cpp
)

add_executable(ether_tests ${ETHER_TEST_SOURCES})

target_include_directories(ether_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
)

target_link_libraries(ether_tests PRIVATE
  ether_core
  doctest::doctest
)

target_compile_options(ether_tests PRIVATE -Wall)

target_compile_definitions(ether_tests PRIVATE
  ETHER_TEST_SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/integration/samples"
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(ether_tests)
