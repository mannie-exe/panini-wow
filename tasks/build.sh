#!/usr/bin/env sh
set -e

BUILD_PRESET="${BUILD_PRESET:-release}"

echo "Building with preset: $BUILD_PRESET"
cmake --build --preset "$BUILD_PRESET"
