
cmake_minimum_required(VERSION 3.16)
project(benzene LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE LIB_SOURCES
  "${CMAKE_SOURCE_DIR}/lib/*.cpp"
  "${CMAKE_SOURCE_DIR}/lib/**/*.cpp"
)

file(GLOB_RECURSE APP_SOURCES
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/**/*.cpp"
)

add_library(benzene_lib ${LIB_SOURCES})

target_include_directories(benzene_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/lib
)

add_executable(benzene ${APP_SOURCES})

target_include_directories(benzene PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(benzene PRIVATE benzene_lib)

