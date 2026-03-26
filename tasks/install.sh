#!/usr/bin/env sh
set -e

# Default TurtleWoW path; override with TURTLEWOW_PATH env var
TURTLEWOW_PATH="${TURTLEWOW_PATH:-$HOME/production/wow/TurtleWoW}"
BUILD_PRESET="${BUILD_PRESET:-cross-mingw32}"
BUILD_DIR="build/$BUILD_PRESET"

if [ ! -d "$TURTLEWOW_PATH" ]; then
    echo "ERROR: TurtleWoW not found at $TURTLEWOW_PATH"
    echo "Set TURTLEWOW_PATH to your TurtleWoW installation directory."
    exit 1
fi

DLL_PATH="$BUILD_DIR/bin/PaniniWoW.dll"
if [ ! -f "$DLL_PATH" ]; then
    echo "ERROR: DLL not found at $DLL_PATH. Run 'mise run build' first."
    exit 1
fi

# Install DLL
echo "Installing PaniniWoW.dll -> $TURTLEWOW_PATH/mods/"
cp "$DLL_PATH" "$TURTLEWOW_PATH/mods/PaniniWoW.dll"

# Install addon
ADDON_SRC="PaniniClassicWoW"
ADDON_DEST="$TURTLEWOW_PATH/Interface/AddOns/PaniniClassicWoW"
echo "Installing addon -> $ADDON_DEST/"
mkdir -p "$ADDON_DEST"
cp "$ADDON_SRC"/*.toc "$ADDON_SRC"/*.lua "$ADDON_DEST/" 2>/dev/null || true

# Check dlls.txt
if ! grep -q "mods/PaniniWoW.dll" "$TURTLEWOW_PATH/dlls.txt" 2>/dev/null; then
    echo ""
    echo "NOTE: Add this line to $TURTLEWOW_PATH/dlls.txt:"
    echo "  mods/PaniniWoW.dll"
fi

echo "Done."
