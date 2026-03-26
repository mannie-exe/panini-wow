#!/usr/bin/env sh
set -e

BUILD_PRESET="${BUILD_PRESET:-cross-mingw32}"

echo "Configuring with preset: $BUILD_PRESET"
cmake --preset "$BUILD_PRESET"
