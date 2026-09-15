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
$RowLabel = '@g8a-timing3'
$RowName = "$Module$RowLabel"
$BlockReport = Join-Path $RepoRoot 'reports\synthesis\zhao_block_fit.json'
$BaselineReceipt = Join-Path $RepoRoot 'reports\synthesis\zhao_g8a_raster_texture_timing2.json'
$TimingReceipt = Join-Path $RepoRoot 'reports\synthesis\zhao_g8a_raster_texture_timing3.json'
$GeneratedFitManifest = Join-Path $RepoRoot 'fpga\rtl\generated\zhao_raster_texture_v3_fit_top.manifest.json'
$RetainedFitManifest = Join-Path $RepoRoot 'reports\synthesis\blockpaths\zhao_raster_texture_v3_fit_top@g8a-timing3.fit.manifest.json'
$TimingRawArtifactParent = Join-Path $RepoRoot 'reports\synthesis\blockpaths'
$TimingRawArtifactFilter = "$RowName.*"
$BaselineReceiptSha256 = '0d82c3a22b6cb14b92b5624cc0e7dde2e7d86fb1068a8e9f52e1a0dbf6c276b7'
$ReceiptToolPaths = @(
    (Join-Path $PSScriptRoot 'g8a_receipt.py'),
    (Join-Path $PSScriptRoot 'g8a_timing3_receipt.py')
)

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
    throw 'G8A timing-batch fit requires a completely clean committed tree.'
}
$headRows = @(& git -C $RepoRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0 -or $headRows.Count -ne 1) {
    throw 'Could not resolve exactly one clean G8A timing-batch source commit.'
}
$head = "$($headRows[0])".Trim()
if ($head -notmatch '^[0-9a-f]{40}$') {
    throw 'The clean G8A timing-batch source commit is malformed.'
}
$branchRows = @(& git -C $RepoRoot symbolic-ref --quiet --short HEAD)
if ($LASTEXITCODE -ne 0 -or $branchRows.Count -ne 1) {
    throw 'G8A timing-batch fit requires a named branch, not a detached HEAD.'
}
$branch = "$($branchRows[0])".Trim()
if ([string]::IsNullOrWhiteSpace($branch)) {
    throw 'G8A timing-batch fit resolved an empty branch name.'
}
$remoteRows = @(& git -C $RepoRoot ls-remote --heads origin "refs/heads/$branch")
if ($LASTEXITCODE -ne 0 -or $remoteRows.Count -ne 1) {
    throw "Could not resolve exactly one origin/$branch head before G8A timing fit."
}
$remoteFields = @($remoteRows[0] -split '\s+')
if ($remoteFields.Count -ne 2 -or $remoteFields[0] -cne $head -or
    $remoteFields[1] -cne "refs/heads/$branch") {
    throw "G8A timing-batch source HEAD is not pushed exactly to origin/$branch."
}

if (-not (Test-Path -LiteralPath $BaselineReceipt -PathType Leaf)) {
    throw 'The preserved timing2 G8A receipt is required before this timing-batch fit.'
}
$baselineHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $BaselineReceipt).Hash.ToLowerInvariant()
if ($baselineHash -cne $BaselineReceiptSha256) {
    throw 'The timing2 baseline receipt bytes differ from the immutable expected digest.'
}
$baseline = Get-Content -LiteralPath $BaselineReceipt -Raw | ConvertFrom-Json
if ($baseline.schema_id -cne 'zhao.g8a.fit_receipt' -or
    $baseline.gate.fit_complete -ne $true -or
    $baseline.gate.structure_pass -ne $true -or
    $baseline.gate.resource_pass -ne $true -or
    $baseline.gate.timing_100mhz_pass -ne $false -or
    $baseline.source.commit -cne 'e3b3cec9bc17348711a3fa9bf999cb8023d4896e') {
    throw 'The timing2 baseline does not describe the exact clean timing-only G8A failure.'
}
if ($head -ceq $baseline.source.commit) {
    throw 'The timing-batch fit source is unchanged from the timing2 baseline.'
}
if (-not (Test-Path -LiteralPath $TimingRawArtifactParent -PathType Container)) {
    throw "The timing3 raw-artifact parent is absent: $TimingRawArtifactParent"
}
$priorRawArtifacts = @(
    Get-ChildItem -LiteralPath $TimingRawArtifactParent `
        -Filter $TimingRawArtifactFilter -File
)
if ($priorRawArtifacts.Count -ne 0) {
    $priorRawNames = @($priorRawArtifacts | ForEach-Object { $_.Name }) -join ', '
    throw "Timing3 raw artifacts already exist ($priorRawNames); preserve and diagnose the partial attempt instead of overwriting it."
}
if (Test-Path -LiteralPath $TimingReceipt) {
    throw "A timing-batch G8A receipt already exists at $TimingReceipt; do not repeat it."
}
if (Test-Path -LiteralPath $RetainedFitManifest) {
    throw "A timing-batch retained fit manifest already exists at $RetainedFitManifest; do not repeat it."
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
$fitManifestBytes = [IO.File]::ReadAllBytes($GeneratedFitManifest)
$fitManifestBase64 = [Convert]::ToBase64String($fitManifestBytes)
$receiptToolHashes = @{}
foreach ($toolPath in $ReceiptToolPaths) {
    if (-not (Test-Path -LiteralPath $toolPath -PathType Leaf)) {
        throw "Required G8A receipt tool is absent: $toolPath"
    }
    $receiptToolHashes[$toolPath] =
        (Get-FileHash -Algorithm SHA256 -LiteralPath $toolPath).Hash.ToLowerInvariant()
}

Write-Host "G8A timing-batch source commit: $head"
Write-Host 'Launching one physical-top-port combined timing-recovery subsystem fit.'
& (Join-Path $PSScriptRoot 'run_block_fit.ps1') `
    -Module $Module `
    -QuartusBin $QuartusBin `
    -TimeoutSeconds $TimeoutSeconds `
    -TopParameters @('ATTR_DSP3=1', 'BILERP_DSP2=1') `
    -VerilogMacros @('ZHAO_DUAL18_CYCLONEV=1') `
    -RowLabel $RowLabel `
    -Seed 1 `
    -PhysicalPins
if (-not $?) {
    throw 'run_block_fit.ps1 failed before a timing-batch receipt could be derived.'
}

foreach ($toolPath in $ReceiptToolPaths) {
    if (-not (Test-Path -LiteralPath $toolPath -PathType Leaf)) {
        throw "A G8A receipt tool disappeared during the fit: $toolPath"
    }
    $toolHashNow =
        (Get-FileHash -Algorithm SHA256 -LiteralPath $toolPath).Hash.ToLowerInvariant()
    if ($toolHashNow -cne $receiptToolHashes[$toolPath]) {
        throw "A G8A receipt tool changed during the fit: $toolPath"
    }
}
$currentManifestBytes = [IO.File]::ReadAllBytes($GeneratedFitManifest)
if ([Convert]::ToBase64String($currentManifestBytes) -cne $fitManifestBase64) {
    throw 'The generated G8A fit manifest changed after the pre-fit snapshot.'
}
$retainedParent = Split-Path -Parent $RetainedFitManifest
if (-not (Test-Path -LiteralPath $retainedParent -PathType Container)) {
    throw "Retained G8A fit-manifest parent is absent: $retainedParent"
}
$retainedTemporary = "$RetainedFitManifest.$([Guid]::NewGuid().ToString('N')).tmp"
try {
    [IO.File]::WriteAllBytes($retainedTemporary, $fitManifestBytes)
    Move-Item -LiteralPath $retainedTemporary -Destination $RetainedFitManifest
} finally {
    if (Test-Path -LiteralPath $retainedTemporary) {
        Remove-Item -LiteralPath $retainedTemporary -Force -Confirm:$false
    }
}

$receiptToolLocks = @()
try {
    foreach ($toolPath in $ReceiptToolPaths) {
        $toolLock = [IO.File]::Open(
            $toolPath, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            [IO.FileShare]::Read)
        $receiptToolLocks += $toolLock
        $lockedHash =
            (Get-FileHash -Algorithm SHA256 -LiteralPath $toolPath).Hash.ToLowerInvariant()
        if ($lockedHash -cne $receiptToolHashes[$toolPath]) {
            throw "A G8A receipt tool changed before its locked invocation: $toolPath"
        }
    }
} catch {
    foreach ($toolLock in $receiptToolLocks) {
        $toolLock.Dispose()
    }
    throw
}

& $PythonExe (Join-Path $PSScriptRoot 'g8a_timing3_receipt.py') --write
$receiptRc = $LASTEXITCODE
foreach ($toolLock in $receiptToolLocks) {
    $toolLock.Dispose()
}
if ($receiptRc -eq 0) {
    Write-Host 'G8A TIMING3 PASS: structure, RAM, resource, and 100-MHz gates all pass.'
} elseif ($receiptRc -eq 2) {
    Write-Host 'G8A TIMING3 measured honestly but FAILED its gate; preserve and diagnose it.'
} else {
    throw 'G8A timing-batch receipt derivation failed closed.'
}
exit $receiptRc
