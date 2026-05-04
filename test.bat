@echo off
setlocal

set "BUILD_DIR=build"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
  cmake -S . -B "%BUILD_DIR%" -G Ninja || exit /b 1
)

cmake --build "%BUILD_DIR%" --target ether_tests || exit /b 1

ctest --test-dir "%BUILD_DIR%" --output-on-failure %*
endlocal
