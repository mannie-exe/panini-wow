$TurtleWoWPath = if ($env:TURTLEWOW_PATH) { $env:TURTLEWOW_PATH } else { "$env:USERPROFILE\production\wow\TurtleWoW" }
$BuildPreset = if ($env:BUILD_PRESET) { $env:BUILD_PRESET } else { "windows" }
$BuildDir = "build/$BuildPreset"

if (-not (Test-Path $TurtleWoWPath)) {
    Write-Error "TurtleWoW not found at $TurtleWoWPath. Set TURTLEWOW_PATH."
    exit 1
}

$DllPath = "$BuildDir/bin/PaniniWoW.dll"
if (-not (Test-Path $DllPath)) {
    Write-Error "DLL not found at $DllPath. Run 'mise run build' first."
    exit 1
}

Write-Host "Installing PaniniWoW.dll -> $TurtleWoWPath\mods\"
Copy-Item $DllPath "$TurtleWoWPath\mods\PaniniWoW.dll" -Force

$AddonDest = "$TurtleWoWPath\Interface\AddOns\PaniniClassicWoW"
Write-Host "Installing addon -> $AddonDest\"
New-Item -ItemType Directory -Force -Path $AddonDest | Out-Null
Copy-Item "PaniniClassicWoW\*" $AddonDest -Force

$DllsTxt = Get-Content "$TurtleWoWPath\dlls.txt" -ErrorAction SilentlyContinue
if ($DllsTxt -notcontains "mods/PaniniWoW.dll") {
    Write-Host ""
    Write-Host "NOTE: Add this line to $TurtleWoWPath\dlls.txt:"
    Write-Host "  mods/PaniniWoW.dll"
}

Write-Host "Done."
