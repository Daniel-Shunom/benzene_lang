#!/usr/bin/env sh
# Install precompiled servers and the compiler; PATH changes remain explicit.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PREFIX=${1:-"$HOME/.local"}
BUILD="$ROOT/build/install-Release"
JOBS=${BENZENE_BUILD_JOBS:-1}
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DETHER_BUILD_TESTS=OFF \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$BUILD/bin" \
  -DETHER_INSTALL_SERVERS=ON -DETHER_BUILD_JOBS="$JOBS"
cmake --build "$BUILD" --parallel "$JOBS"
cmake --install "$BUILD" --prefix "$PREFIX" --config Release
printf 'Installed Benzene. Add %s/bin to PATH.\n' "$PREFIX"
