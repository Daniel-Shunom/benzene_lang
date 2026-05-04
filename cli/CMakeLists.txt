file(GLOB_RECURSE ETHER_CLI_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(ether ${ETHER_CLI_SOURCES})

target_include_directories(ether PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ether PRIVATE ether_core)

target_compile_options(ether PRIVATE -Wall)
