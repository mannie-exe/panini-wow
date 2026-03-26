$BuildPreset = if ($env:BUILD_PRESET) { $env:BUILD_PRESET } else { "windows" }
$BuildDir = "build/$BuildPreset"

if (-not (Test-Path "$BuildDir/build.ninja")) {
    Write-Error "Not configured. Run 'mise run configure' first."
    exit 1
}

Write-Host "Compiling shaders..."
cmake --build $BuildDir --target shaders
Write-Host "Shader bytecode generated at: $BuildDir/generated/panini_shader.h"
