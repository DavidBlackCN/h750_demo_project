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

if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "Local tool configuration is missing: $configPath. Copy Tools/config/local.ps1.example to local.ps1 and set the paths."
}

# Always build first. Never program an ELF left by an earlier failed build.
& (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset -Reconfigure:$Reconfigure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$localConfig = & $configPath
foreach ($name in 'OpenOcdPath', 'OpenOcdScripts') {
    if (-not $localConfig.ContainsKey($name) -or -not $localConfig[$name]) {
        throw "Tools/config/local.ps1 must define $name."
    }
}
foreach ($path in @($localConfig.OpenOcdPath, $localConfig.OpenOcdScripts)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Configured path does not exist: $path"
    }
}

$elfPath = Join-Path $repoRoot "build/$Preset/adc_fft_demo.elf"
if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf)) {
    throw "Build reported success but ELF was not found: $elfPath"
}
$openOcdElfPath = $elfPath.Replace('\', '/')

$hadNativeErrorPreference = Test-Path -LiteralPath 'Variable:PSNativeCommandUseErrorActionPreference'
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
if ($hadNativeErrorPreference) {
    $previousNativeErrorPreference = $PSNativeCommandUseErrorActionPreference
    # OpenOCD prints its normal banner and diagnostics to stderr.  Its process
    # exit code and captured text below are the authoritative result.
    $PSNativeCommandUseErrorActionPreference = $false
}
try {
    $openOcdOutput = & $localConfig.OpenOcdPath `
        -s $localConfig.OpenOcdScripts `
        -f 'interface/cmsis-dap.cfg' `
        -f 'target/stm32h7x.cfg' `
        -c "program `"$openOcdElfPath`" verify reset exit" 2>&1
    $openOcdExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
    if ($hadNativeErrorPreference) {
        $PSNativeCommandUseErrorActionPreference = $previousNativeErrorPreference
    }
}
$openOcdOutput | ForEach-Object { Write-Host $_ }

if ($openOcdExitCode -ne 0) { exit $openOcdExitCode }
$outputText = $openOcdOutput | Out-String
foreach ($pattern in 'Programming Finished', 'Verified OK', 'Resetting Target') {
    if ($outputText -notmatch [regex]::Escape($pattern)) {
        throw "OpenOCD exited successfully but did not report '$pattern'; do not treat this as a completed flash."
    }
}

Write-Host "Flash completed: $elfPath"
