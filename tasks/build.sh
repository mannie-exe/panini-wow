#!/usr/bin/env sh
set -e

CONFIGURE_PRESET="${CONFIGURE_PRESET:-cross-mingw32}"
BUILD_PRESET="${BUILD_PRESET:-release}"

if [ ! -d "build/${CONFIGURE_PRESET}" ]; then
    echo "Configuring with preset: $CONFIGURE_PRESET"
    cmake --preset "$CONFIGURE_PRESET"
fi

echo "Building with preset: $BUILD_PRESET"
cmake --build --preset "$BUILD_PRESET"
