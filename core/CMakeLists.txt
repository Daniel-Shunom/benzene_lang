file(GLOB_RECURSE ETHER_CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(ether_core STATIC ${ETHER_CORE_SOURCES})

target_include_directories(ether_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(ether_core PRIVATE -Wall)
