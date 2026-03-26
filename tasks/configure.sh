#!/usr/bin/env sh
set -e

CONFIGURE_PRESET="${CONFIGURE_PRESET:-cross-mingw32}"

echo "Configuring with preset: $CONFIGURE_PRESET"
cmake --preset "$CONFIGURE_PRESET"
