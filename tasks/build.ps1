$BuildPreset = if ($env:BUILD_PRESET) { $env:BUILD_PRESET } else { "windows" }
Write-Host "Building with preset: $BuildPreset"
cmake --build --preset $BuildPreset
