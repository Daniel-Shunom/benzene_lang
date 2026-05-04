#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_CXX_COMPILER=g++
fi

cmake --build "$BUILD_DIR" --target ether_tests

ctest --test-dir "$BUILD_DIR" --output-on-failure "$@"
