[CmdletBinding()]
param(
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [string]$PythonExe = 'C:\Users\Fabs\AppData\Local\Programs\Python\Python312\python.exe',
    [int]$TimeoutSeconds = 28800
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Module = 'zhao_raster_texture_v3_fit_top'
$RowName = "$Module@g8a"
$BlockReport = Join-Path $RepoRoot 'reports\synthesis\zhao_block_fit.json'
$Receipt = Join-Path $RepoRoot 'reports\synthesis\zhao_g8a_raster_texture.json'

if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) {
    throw "Pinned Python executable is absent: $PythonExe"
}
if ($TimeoutSeconds -lt 60) {
    throw '-TimeoutSeconds must be at least 60 seconds.'
}
$quartusProcesses = @(Get-Process -Name 'quartus*' -ErrorAction SilentlyContinue)
if ($quartusProcesses.Count -ne 0) {
    throw 'Another Quartus process is already running; G8A is a serial subsystem fit.'
}

$dirty = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain) -join "`n"
if (-not [string]::IsNullOrWhiteSpace($dirty)) {
    Write-Host $dirty
    throw 'G8A requires a completely clean committed tree before source capture.'
}
$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not resolve the clean G8A source commit.'
}

if (Test-Path -LiteralPath $Receipt) {
    throw "A G8A receipt already exists at $Receipt; the one-fit packet cannot silently repeat."
}
if (Test-Path -LiteralPath $BlockReport) {
    $report = Get-Content -LiteralPath $BlockReport -Raw | ConvertFrom-Json
    $prior = @($report.blocks | Where-Object { $_.module -ceq $RowName })
    if ($prior.Count -ne 0) {
        throw "A $RowName row already exists; preserve and diagnose it instead of rerunning."
    }
}

& $PythonExe (Join-Path $PSScriptRoot 'gen_raster_texture_v3_fit_top.py') --check
if ($LASTEXITCODE -ne 0) { throw 'G8A generated wrapper/manifest is stale.' }
& $PythonExe (Join-Path $RepoRoot 'tests\tools\test_raster_texture_v3_fit_top.py') -q
if ($LASTEXITCODE -ne 0) { throw 'G8A static pre-fit controls failed.' }

Write-Host "G8A source commit: $head"
Write-Host 'Launching the one physical-top-port raster/texture subsystem fit.'
& (Join-Path $PSScriptRoot 'run_block_fit.ps1') `
    -Module $Module `
    -QuartusBin $QuartusBin `
    -TimeoutSeconds $TimeoutSeconds `
    -RowLabel '@g8a' `
    -Seed 1 `
    -PhysicalPins
if (-not $?) { throw 'run_block_fit.ps1 failed before a G8A receipt could be derived.' }

& $PythonExe (Join-Path $PSScriptRoot 'g8a_receipt.py') --write
$receiptRc = $LASTEXITCODE
if ($receiptRc -eq 0) {
    Write-Host 'G8A PASS: clean connected receipt meets structure, RAM, resource, and 100-MHz gates.'
} elseif ($receiptRc -eq 2) {
    Write-Host 'G8A measured honestly but FAILED its gate; preserve the receipt and diagnose it.'
} else {
    throw 'G8A receipt derivation failed closed.'
}
exit $receiptRc
