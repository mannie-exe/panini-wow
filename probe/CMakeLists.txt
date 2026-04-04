cmake_minimum_required(VERSION 3.25)
project(Probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(Probe SHARED src/probe.cpp)

target_link_libraries(Probe PRIVATE
    d3d9
    psapi
    user32
    kernel32
)

if(MINGW)
    target_link_options(Probe PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--enable-stdcall-fixup
    )
endif()

set_target_properties(Probe PROPERTIES
    PREFIX ""
    OUTPUT_NAME "Probe"
)
