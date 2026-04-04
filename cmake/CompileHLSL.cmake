# CompileHLSL.cmake
#
# Compiles HLSL shaders to D3D9 bytecode C headers using fxc2.
#
# fxc2 is a MinGW-compiled wrapper around d3dcompiler_47.dll (Mozilla, MIT-ish).
# Source vendored at tools/fxc2/ with the 32-bit d3dcompiler_47.dll.
#
# On macOS/Linux: fxc2.exe runs under Wine (hard build dependency).
# On Windows: fxc2.exe runs natively.

set(FXC2_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/fxc2")
set(FXC2_SOURCE "${FXC2_SOURCE_DIR}/fxc2.cpp")
set(FXC2_DLL "${FXC2_SOURCE_DIR}/d3dcompiler_47.dll")
set(FXC2_TOOL_DIR "${CMAKE_BINARY_DIR}/tools")
set(FXC2_EXE "${FXC2_TOOL_DIR}/fxc2.exe")
set(FXC2_DLL_DEST "${FXC2_TOOL_DIR}/d3dcompiler_47.dll")

file(MAKE_DIRECTORY "${FXC2_TOOL_DIR}")

# Build fxc2.exe with the project's cross-compiler, copy DLL beside it
add_custom_command(
    OUTPUT "${FXC2_EXE}" "${FXC2_DLL_DEST}"
    COMMAND ${CMAKE_CXX_COMPILER}
        "${FXC2_SOURCE}"
        -o "${FXC2_EXE}"
        -static
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FXC2_DLL}" "${FXC2_DLL_DEST}"
    DEPENDS "${FXC2_SOURCE}" "${FXC2_DLL}"
    COMMENT "Building fxc2.exe (HLSL shader compiler)"
    VERBATIM
)

add_custom_target(fxc2_tool DEPENDS "${FXC2_EXE}" "${FXC2_DLL_DEST}")

# Invoke: directly on Windows, via Wine otherwise
if(CMAKE_HOST_WIN32)
    set(FXC2_COMMAND "${FXC2_EXE}")
else()
    # Prefer wineloader over wine: CrossOver's wine binary requires the
    # bottle system and ignores WINEPREFIX, but wineloader is the actual
    # Wine runtime underneath and respects WINEPREFIX normally.
    find_program(WINE_EXECUTABLE NAMES wineloader wine wine64 REQUIRED)
    set(FXC2_COMMAND ${CMAKE_COMMAND} -E env
        "WINEPREFIX=${CMAKE_SOURCE_DIR}/.wine"
        "WINEDEBUG=-all"
        "${WINE_EXECUTABLE}" "${FXC2_EXE}")
endif()

# compile_hlsl(SOURCE ... OUTPUT ... ENTRY ... PROFILE ... VARNAME ...)
function(compile_hlsl)
    cmake_parse_arguments(HLSL "" "SOURCE;OUTPUT;ENTRY;PROFILE;VARNAME" "" ${ARGN})

    if(NOT HLSL_SOURCE)
        message(FATAL_ERROR "compile_hlsl: SOURCE is required")
    endif()
    if(NOT HLSL_OUTPUT)
        message(FATAL_ERROR "compile_hlsl: OUTPUT is required")
    endif()
    if(NOT HLSL_ENTRY)
        set(HLSL_ENTRY "main")
    endif()
    if(NOT HLSL_PROFILE)
        set(HLSL_PROFILE "ps_3_0")
    endif()
    if(NOT HLSL_VARNAME)
        set(HLSL_VARNAME "g_shaderBytecode")
    endif()

    get_filename_component(HLSL_SOURCE_ABS "${HLSL_SOURCE}" ABSOLUTE)
    get_filename_component(HLSL_OUTPUT_DIR "${HLSL_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${HLSL_OUTPUT_DIR}")

    add_custom_command(
        OUTPUT "${HLSL_OUTPUT}"
        COMMAND ${FXC2_COMMAND}
            --nologo
            -T ${HLSL_PROFILE}
            -E ${HLSL_ENTRY}
            -Vn${HLSL_VARNAME}
            "-Fh${HLSL_OUTPUT}"
            "${HLSL_SOURCE_ABS}"
        DEPENDS "${HLSL_SOURCE_ABS}" "${FXC2_EXE}" "${FXC2_DLL_DEST}"
        COMMENT "HLSL: ${HLSL_SOURCE} -> ${HLSL_OUTPUT} (${HLSL_PROFILE})"
        VERBATIM
    )
endfunction()
