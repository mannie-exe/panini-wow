# CMake toolchain file for cross-compiling PE32 (32-bit Windows) DLLs
# using MinGW-w64 from macOS or Linux.
#
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake
#
# Requires: i686-w64-mingw32-gcc installed (e.g. `brew install mingw-w64`)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# Find the cross-compiler prefix. Homebrew on Apple Silicon installs to
# /opt/homebrew/bin, Intel to /usr/local/bin, Linux to /usr/bin.
# CMake will search PATH automatically when we set the compiler names.
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# Static link libgcc/libstdc++ so the DLL has no MinGW runtime dependencies
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# Search behavior: programs on host, libraries/headers in target sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
