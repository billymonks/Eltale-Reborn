# Copies the ray tracing configuration from the authoring folder (Documents\Quest64Recompiled\Ray Tracing) into
# assets\materials, where it ships with the game: rt_materials.json, rt_emitters.json and rt_lights.json.
# The per-machine "_location" and "_saved" stamps the HTML editors use are left out. The Documents files are only read.
#
# Usage, from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\export_rt_config.ps1
#   powershell -ExecutionPolicy Bypass -File tools\export_rt_config.ps1 -Source "D:\Somewhere\Ray Tracing"

param(
    [string]$Source = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Quest64Recompiled\Ray Tracing'),
    [string]$Destination = (Join-Path (Split-Path -Parent $PSScriptRoot) 'assets\materials')
)

$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding $false
$files = 'rt_materials.json', 'rt_emitters.json', 'rt_lights.json'

foreach ($name in $files) {
    $from = Join-Path $Source $name
    if (-not (Test-Path $from)) {
        Write-Warning "$from not found; $name in $Destination is left as it is."
        continue
    }

    # Drop the top-level stamp lines (each is followed by more keys, so it ends with a comma).
    $lines = [IO.File]::ReadAllLines($from, $utf8) | Where-Object { $_ -notmatch '^\s{0,4}"_(location|saved)"\s*:.*,\s*$' }
    $text = ($lines -join "`n") + "`n"
    $document = $text | ConvertFrom-Json

    $to = Join-Path $Destination $name
    [IO.File]::WriteAllText($to, $text, $utf8)

    $counts = @()
    foreach ($section in 'materials', 'emitters', 'candidates', 'lights', 'room settings') {
        $value = $document.$section
        if ($null -ne $value) {
            $count = if ($value -is [array]) { $value.Count } else { @($value.PSObject.Properties).Count }
            $counts += "$section $count"
        }
    }

    $size = [Math]::Round((Get-Item $to).Length / 1KB)
    Write-Host ("{0}: {1} ({2} KB)" -f $name, ($counts -join ', '), $size)
}

Write-Host "Shipped configuration written to $Destination. Rebuild so the build copies it next to the game."
