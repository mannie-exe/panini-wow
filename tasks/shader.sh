#!/usr/bin/env sh
set -e

BUILD_PRESET="${BUILD_PRESET:-cross-mingw32}"
BUILD_DIR="build/$BUILD_PRESET"

# Ensure configured
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    echo "Not configured. Run 'mise run configure' first."
    exit 1
fi

echo "Compiling shaders..."
cmake --build "$BUILD_DIR" --target shaders
echo "Shader bytecode generated at: $BUILD_DIR/generated/panini_shader.h"
