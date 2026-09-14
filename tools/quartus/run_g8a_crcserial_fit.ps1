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
$RowLabel = '@g8a-crcserial'
$RowName = "$Module$RowLabel"
$BlockReport = Join-Path $RepoRoot 'reports\synthesis\zhao_block_fit.json'
$BaselineReceipt = Join-Path $RepoRoot 'reports\synthesis\zhao_g8a_raster_texture.json'
$RepairReceipt = Join-Path $RepoRoot 'reports\synthesis\zhao_g8a_raster_texture_crcserial.json'

if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) {
    throw "Pinned Python executable is absent: $PythonExe"
}
if ($TimeoutSeconds -lt 60) {
    throw '-TimeoutSeconds must be at least 60 seconds.'
}
if (@(Get-Process -Name 'quartus*' -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Another Quartus process is already running; G8A is a serial subsystem fit.'
}

$dirty = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain) -join "`n"
if (-not [string]::IsNullOrWhiteSpace($dirty)) {
    Write-Host $dirty
    throw 'G8A CRC-serialization fit requires a completely clean committed tree.'
}
$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not resolve the clean G8A CRC-serialization source commit.'
}

if (-not (Test-Path -LiteralPath $BaselineReceipt -PathType Leaf)) {
    throw 'The preserved failed baseline G8A receipt is required before this repair fit.'
}
$baseline = Get-Content -LiteralPath $BaselineReceipt -Raw | ConvertFrom-Json
if ($baseline.schema_id -cne 'zhao.g8a.fit_receipt' -or
    $baseline.gate.fit_complete -ne $true -or
    $baseline.gate.structure_pass -ne $true -or
    $baseline.gate.resource_pass -ne $true -or
    $baseline.gate.timing_100mhz_pass -ne $false -or
    $baseline.source.commit -cne 'c88e2b317c981a98f177a2c049688af1c9e30ba3') {
    throw 'The preserved baseline does not describe the exact clean timing-only G8A failure.'
}
if (Test-Path -LiteralPath $RepairReceipt) {
    throw "A CRC-serialization G8A receipt already exists at $RepairReceipt; do not repeat it."
}
$prior = @()
if (Test-Path -LiteralPath $BlockReport) {
    $report = Get-Content -LiteralPath $BlockReport -Raw | ConvertFrom-Json
    $prior = @($report.blocks | Where-Object { $_.module -ceq $RowName })
}
if ($prior.Count -ne 0) {
    throw "A $RowName row already exists; preserve and diagnose it instead of rerunning."
}

& $PythonExe (Join-Path $PSScriptRoot 'gen_raster_texture_v3_fit_top.py') --check
if ($LASTEXITCODE -ne 0) { throw 'G8A generated wrapper/manifest is stale.' }
& $PythonExe (Join-Path $RepoRoot 'tests\tools\test_raster_texture_v3_fit_top.py') -q
if ($LASTEXITCODE -ne 0) { throw 'G8A static pre-fit controls failed.' }

Write-Host "G8A CRC-serialization source commit: $head"
Write-Host 'Launching one physical-top-port post-diagnosis subsystem fit.'
& (Join-Path $PSScriptRoot 'run_block_fit.ps1') `
    -Module $Module `
    -QuartusBin $QuartusBin `
    -TimeoutSeconds $TimeoutSeconds `
    -RowLabel $RowLabel `
    -Seed 1 `
    -PhysicalPins
if (-not $?) {
    throw 'run_block_fit.ps1 failed before a CRC-serialization receipt could be derived.'
}

& $PythonExe (Join-Path $PSScriptRoot 'g8a_crcserial_receipt.py') --write
$receiptRc = $LASTEXITCODE
if ($receiptRc -eq 0) {
    Write-Host 'G8A CRC-SERIAL PASS: structure, RAM, resource, and 100-MHz gates all pass.'
} elseif ($receiptRc -eq 2) {
    Write-Host 'G8A CRC-SERIAL measured honestly but FAILED its gate; preserve and diagnose it.'
} else {
    throw 'G8A CRC-serialization receipt derivation failed closed.'
}
exit $receiptRc
