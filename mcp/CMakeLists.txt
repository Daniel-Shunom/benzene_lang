file(GLOB_RECURSE MCP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(mcp STATIC ${MCP_SOURCES})

target_include_directories(mcp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(mcp PRIVATE -Wall)
