file(GLOB_RECURSE ETHER_CLI_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(ether ${ETHER_CLI_SOURCES})

target_include_directories(ether PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ether PRIVATE ether_core)
target_compile_definitions(ether PRIVATE ETHER_VERSION="${PROJECT_VERSION}")

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # MinGW's std::print terminal support lives in libstdc++exp.
  target_link_libraries(ether PRIVATE stdc++exp)
endif()

target_compile_options(ether PRIVATE -Wall)
