$BuildPreset = if ($env:BUILD_PRESET) { $env:BUILD_PRESET } else { "windows" }
Write-Host "Configuring with preset: $BuildPreset"
cmake --preset $BuildPreset
