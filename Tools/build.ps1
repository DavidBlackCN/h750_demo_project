[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Preset = 'Debug',
    [switch]$Reconfigure
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $PSScriptRoot 'config/local.ps1'
$examplePath = Join-Path $PSScriptRoot 'config/local.ps1.example'

if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "Local tool configuration is missing: $configPath. Copy $examplePath to local.ps1 and set the paths."
}

$localConfig = & $configPath
foreach ($name in 'CubeCmakePath', 'CubeBinDir') {
    if (-not $localConfig.ContainsKey($name) -or -not $localConfig[$name]) {
        throw "Tools/config/local.ps1 must define $name."
    }
}
foreach ($path in @($localConfig.CubeCmakePath, $localConfig.CubeBinDir)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Configured path does not exist: $path"
    }
}

$env:PATH = "$($localConfig.CubeBinDir)$([IO.Path]::PathSeparator)$env:PATH"
$cachePath = Join-Path $repoRoot "build/$Preset/CMakeCache.txt"

Push-Location -LiteralPath $repoRoot
try {
    if ($Reconfigure -or -not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        & $localConfig.CubeCmakePath --preset $Preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    & $localConfig.CubeCmakePath --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
