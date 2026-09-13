[CmdletBinding()]
param(
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [string]$BuildDirectory,
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$expectedCheckout = 'zhaozhou-board-bringup-20260913'
$expectedBranch = 'zhaozhou-board-bringup-20260913'

if ((Split-Path $repoRoot -Leaf) -ne $expectedCheckout) {
    throw "Specs build must run from the dedicated $expectedCheckout checkout, not $repoRoot"
}
$branch = (& git -C $repoRoot branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -ne $expectedBranch) {
    throw "Specs build requires branch $expectedBranch; current branch is '$branch'"
}

$ownedPaths = @(
    '.gitattributes',
    'fpga/sys',
    'fpga/ZhaozhouSpecs.qpf',
    'fpga/ZhaozhouSpecs.qsf',
    'fpga/ZhaozhouSpecs.sdc',
    'fpga/files_specs.qip',
    'fpga/rtl/common/zhao_crc32c_fold.sv',
    'fpga/rtl/raster/zhao_raster_fill.sv',
    'fpga/rtl/common/zhao_dual18_mul.sv',
    'fpga/rtl/platform/zhao_ssone_spec_tests.sv',
    'fpga/rtl/platform/zhao_ssone_specs_emu.sv',
    'fpga/rtl/pll.qip',
    'fpga/rtl/pll.v',
    'fpga/rtl/pll',
    'tests/board',
    'tools/board'
)
$dirty = @(& git -C $repoRoot -c core.autocrlf=true status --porcelain -- $ownedPaths)
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect hardware-spec source status.' }
if ($dirty.Count -ne 0) {
    throw "Commit the hardware-spec source before compiling so the RBF has exact provenance:`n$($dirty -join "`n")"
}

$verifyScript = Join-Path $repoRoot 'tools\board\verify_superstation_specs.py'
& python $verifyScript --repo $repoRoot
if ($LASTEXITCODE -ne 0) { throw 'SuperStation hardware-spec source preflight failed.' }
& python (Join-Path $repoRoot 'tests\board\run_ssone_spec_tests.py')
if ($LASTEXITCODE -ne 0) { throw 'SuperStation hardware-spec directed vectors failed.' }

$quartus = Join-Path $QuartusBin 'quartus_sh.exe'
if (-not (Test-Path -LiteralPath $quartus -PathType Leaf)) {
    throw "Required Quartus executable not found: $quartus"
}
$otherQuartus = @(Get-Process -Name 'quartus*' -ErrorAction SilentlyContinue)
if ($otherQuartus.Count -ne 0) {
    $details = $otherQuartus | ForEach-Object { "$($_.Id) $($_.ProcessName)" }
    throw "Another Quartus process is already running; do not contend with its fit:`n$($details -join "`n")"
}
if ((Get-Date).Hour -ge 22) {
    throw "Do not start this board fit in the separate lane's pinned 23:00 shell-fit window."
}

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot 'build-board-superstation-specs'
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot $BuildDirectory
}
$buildRoot = [System.IO.Path]::GetFullPath($BuildDirectory)
$marker = Join-Path $buildRoot '.zhaozhou-superstation-specs-build'

if (Test-Path -LiteralPath $buildRoot) {
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw "Refusing to reuse unowned build directory: $buildRoot"
    }
    if (-not $Clean) {
        throw "Build directory already exists. Re-run with -Clean to replace this owned build: $buildRoot"
    }
    Remove-Item -LiteralPath $buildRoot -Recurse -Force -Confirm:$false
}

foreach ($directory in @('rtl\common', 'rtl\raster', 'rtl\platform')) {
    New-Item -ItemType Directory -Path (Join-Path $buildRoot $directory) -Force | Out-Null
}
foreach ($name in @('ZhaozhouSpecs.qpf', 'ZhaozhouSpecs.qsf', 'ZhaozhouSpecs.sdc', 'files_specs.qip')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "fpga\$name") -Destination $buildRoot
}
foreach ($relative in @(
    'rtl\common\zhao_crc32c_fold.sv',
    'rtl\raster\zhao_raster_fill.sv',
    'rtl\common\zhao_dual18_mul.sv',
    'rtl\platform\zhao_ssone_spec_tests.sv',
    'rtl\platform\zhao_ssone_specs_emu.sv'
)) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "fpga\$relative") -Destination (Join-Path $buildRoot $relative)
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll.qip') -Destination (Join-Path $buildRoot 'rtl\pll.qip')
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll.v') -Destination (Join-Path $buildRoot 'rtl\pll.v')
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll') -Destination (Join-Path $buildRoot 'rtl\pll') -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\sys') -Destination (Join-Path $buildRoot 'sys') -Recurse

$sourceCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
[System.IO.File]::WriteAllText(
    $marker,
    "sourceCommit=$sourceCommit`ncreated=$((Get-Date).ToString('o'))`n",
    (New-Object System.Text.UTF8Encoding($false))
)

. (Join-Path $repoRoot 'tools\env\zhao-env.ps1')
Push-Location $buildRoot
try {
    & $quartus --flow compile ZhaozhouSpecs
    $compileRc = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($compileRc -ne 0) {
    throw "Quartus specs compile failed with exit code $compileRc; reports remain in $buildRoot"
}

& python $verifyScript --repo $repoRoot --build-dir $buildRoot --json
if ($LASTEXITCODE -ne 0) { throw 'SuperStation hardware-spec post-build audit failed.' }

Write-Host "Built and audited: $buildRoot\output_files\ZhaozhouSpecs.rbf" -ForegroundColor Green
