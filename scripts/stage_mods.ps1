# Copy the current Eltale Reborn and Modern Gameplay mods into mods/.
$ErrorActionPreference = "Stop"
$dest = Join-Path $PSScriptRoot "..\mods"
$build = Join-Path $PSScriptRoot "..\..\Quest-64-Good-Edition\build"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path $build "eltale_reborn.nrm") (Join-Path $dest "eltale_reborn.nrm") -Force
Copy-Item (Join-Path $build "gameplay\quest64_modern_gameplay.nrm") (Join-Path $dest "quest64_modern_gameplay.nrm") -Force
Write-Output "Staged mods into $dest"
