cmake_minimum_required(VERSION 3.25)

project(PaniniWoW
    VERSION 0.1.0
    DESCRIPTION "Panini projection post-process mod for WoW 1.12.1"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompileHLSL)

#------------------------------------------------------------------------------
# Shader Compilation
#------------------------------------------------------------------------------
set(PANINI_SHADER_HEADER "${CMAKE_BINARY_DIR}/generated/panini_shader.h")

compile_hlsl(
    SOURCE   "${CMAKE_SOURCE_DIR}/shaders/panini.hlsl"
    OUTPUT   "${PANINI_SHADER_HEADER}"
    ENTRY    main
    PROFILE  ps_3_0
    VARNAME  g_paniniShaderBytecode
)

set(DEBUG_SHADER_HEADER "${CMAKE_BINARY_DIR}/generated/debug_shader.h")

compile_hlsl(
    SOURCE   "${CMAKE_SOURCE_DIR}/shaders/debug.hlsl"
    OUTPUT   "${DEBUG_SHADER_HEADER}"
    ENTRY    main
    PROFILE  ps_3_0
    VARNAME  g_debugShaderBytecode
)

# Custom target so we can build shaders independently: cmake --build build --target shaders
add_custom_target(shaders DEPENDS "${PANINI_SHADER_HEADER}" "${DEBUG_SHADER_HEADER}")

#------------------------------------------------------------------------------
# PaniniWoW DLL
#------------------------------------------------------------------------------
add_library(PaniniWoW SHARED
    src/dllmain.cpp
    src/hooks.cpp
    src/panini.cpp
    src/cvar.cpp
    src/state.cpp
    src/log.cpp
)

target_include_directories(PaniniWoW PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_BINARY_DIR}/generated"
)

# Shader bytecode headers are generated dependencies
target_sources(PaniniWoW PRIVATE "${PANINI_SHADER_HEADER}" "${DEBUG_SHADER_HEADER}")
add_dependencies(PaniniWoW shaders)

option(PANINI_DEBUG_LOG "Enable debug logging (LOG_DEBUG calls)" OFF)
if(PANINI_DEBUG_LOG)
    target_compile_definitions(PaniniWoW PRIVATE PANINI_DEBUG_LOG=1)
endif()

# Link against D3D9 (provided by MinGW-w64 sysroot or Windows SDK)
target_link_libraries(PaniniWoW PRIVATE
    d3d9
    user32
    kernel32
)

# Static link MinGW runtimes so DLL has zero external dependencies
if(MINGW)
    target_link_options(PaniniWoW PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--enable-stdcall-fixup
    )
endif()

# Strip symbols in release for smaller DLL
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    if(MINGW)
        target_link_options(PaniniWoW PRIVATE -s)
    endif()
endif()

# DLL does not need a lib prefix on Windows
set_target_properties(PaniniWoW PROPERTIES
    PREFIX ""
    OUTPUT_NAME "PaniniWoW"
)

#------------------------------------------------------------------------------
# Install target (copies DLL to a staging directory)
#------------------------------------------------------------------------------
install(TARGETS PaniniWoW
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

# Also install the addon
install(DIRECTORY "${PROJECT_SOURCE_DIR}/PaniniClassicWoW/"
    DESTINATION addons/PaniniClassicWoW
)

#------------------------------------------------------------------------------
# Tests (native, no cross-compilation)
#------------------------------------------------------------------------------
option(PANINI_BUILD_TESTS "Build PaniniWoW tests" ON)

if(PANINI_BUILD_TESTS)
    include(cmake/CPM.cmake)
    include(PaniniGoogleTest)

    enable_testing()
    add_executable(PaniniTests tests/TestMain.cc)
    target_link_libraries(PaniniTests PRIVATE
        GTest::gtest
        GTest::gmock
    )
    target_include_directories(PaniniTests PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
    )
    add_subdirectory(tests)
    include(GoogleTest)
    gtest_discover_tests(PaniniTests)
endif()
