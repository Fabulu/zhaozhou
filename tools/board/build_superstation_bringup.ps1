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
    throw "Board build must run from the dedicated $expectedCheckout checkout, not $repoRoot"
}

$branch = (& git -C $repoRoot branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -ne $expectedBranch) {
    throw "Board build requires branch $expectedBranch; current branch is '$branch'"
}

$ownedPaths = @(
    '.gitattributes',
    'fpga/sys',
    'fpga/ZhaozhouBringup.qpf',
    'fpga/ZhaozhouBringup.qsf',
    'fpga/ZhaozhouBringup.sdc',
    'fpga/files_bringup.qip',
    'fpga/rtl/platform/zhao_ssone_bringup.sv',
    'fpga/rtl/pll.qip',
    'fpga/rtl/pll.v',
    'fpga/rtl/pll',
    'tools/board'
)
$dirty = @(& git -C $repoRoot -c core.autocrlf=true status --porcelain -- $ownedPaths)
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect board source status.' }
if ($dirty.Count -ne 0) {
    throw "Commit the board source before compiling so the RBF has exact provenance:`n$($dirty -join "`n")"
}

$verifyScript = Join-Path $repoRoot 'tools\board\verify_superstation_bringup.py'
& python $verifyScript --repo $repoRoot
if ($LASTEXITCODE -ne 0) { throw 'SuperStation source preflight failed.' }

$quartus = Join-Path $QuartusBin 'quartus_sh.exe'
if (-not (Test-Path -LiteralPath $quartus -PathType Leaf)) {
    throw "Required Quartus executable not found: $quartus"
}

$otherQuartus = @(Get-Process -Name 'quartus*' -ErrorAction SilentlyContinue)
if ($otherQuartus.Count -ne 0) {
    $details = $otherQuartus | ForEach-Object { "$($_.Id) $($_.ProcessName)" }
    throw "Another Quartus process is already running; do not contend with its fit:`n$($details -join "`n")"
}

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot 'build-board-superstation'
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot $BuildDirectory
}
$buildRoot = [System.IO.Path]::GetFullPath($BuildDirectory)
$marker = Join-Path $buildRoot '.zhaozhou-superstation-build'

if (Test-Path -LiteralPath $buildRoot) {
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw "Refusing to reuse unowned build directory: $buildRoot"
    }
    if (-not $Clean) {
        throw "Build directory already exists. Re-run with -Clean to replace this owned build: $buildRoot"
    }
    Remove-Item -LiteralPath $buildRoot -Recurse -Force -Confirm:$false
}

New-Item -ItemType Directory -Path (Join-Path $buildRoot 'rtl\platform') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\ZhaozhouBringup.qpf') -Destination $buildRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\ZhaozhouBringup.qsf') -Destination $buildRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\ZhaozhouBringup.sdc') -Destination $buildRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\files_bringup.qip') -Destination $buildRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\platform\zhao_ssone_bringup.sv') `
              -Destination (Join-Path $buildRoot 'rtl\platform\zhao_ssone_bringup.sv')
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll.qip') -Destination (Join-Path $buildRoot 'rtl\pll.qip')
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll.v') -Destination (Join-Path $buildRoot 'rtl\pll.v')
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\rtl\pll') -Destination (Join-Path $buildRoot 'rtl\pll') -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot 'fpga\sys') -Destination (Join-Path $buildRoot 'sys') -Recurse

$patchScript = Join-Path $repoRoot 'tools\board\patch_mister_sys_top.py'
& python $patchScript (Join-Path $buildRoot 'sys\sys_top.v') --json
if ($LASTEXITCODE -ne 0) { throw 'SuperStation sys_top safety overlay failed.' }

$sourceCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
[System.IO.File]::WriteAllText(
    $marker,
    "sourceCommit=$sourceCommit`ncreated=$((Get-Date).ToString('o'))`n",
    (New-Object System.Text.UTF8Encoding($false))
)

$manifestScript = Join-Path $repoRoot 'tools\board\superstation_build_manifest.py'
$sourceManifest = Join-Path $buildRoot 'ZhaozhouBringup.source-manifest.json'
$completeManifest = Join-Path $buildRoot 'ZhaozhouBringup.complete-manifest.json'
& python $manifestScript create --profile Bringup --phase source --repo $repoRoot `
    --build-dir $buildRoot --output $sourceManifest
if ($LASTEXITCODE -ne 0) { throw 'SuperStation source manifest creation failed.' }

. (Join-Path $repoRoot 'tools\env\zhao-env.ps1')

Push-Location $buildRoot
try {
    & $quartus --flow compile ZhaozhouBringup
    $compileRc = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($compileRc -ne 0) {
    throw "Quartus compile failed with exit code $compileRc; reports remain in $buildRoot"
}

& python $verifyScript --repo $repoRoot --build-dir $buildRoot --json
if ($LASTEXITCODE -ne 0) { throw 'SuperStation post-build audit failed.' }
& python $manifestScript create --profile Bringup --phase complete --repo $repoRoot `
    --build-dir $buildRoot --source-manifest $sourceManifest --output $completeManifest
if ($LASTEXITCODE -ne 0) { throw 'SuperStation complete manifest creation failed.' }
& python $manifestScript verify --manifest $completeManifest --repo $repoRoot --build-dir $buildRoot
if ($LASTEXITCODE -ne 0) { throw 'SuperStation complete manifest verification failed.' }

Write-Host "Built and audited: $buildRoot\output_files\ZhaozhouBringup.rbf" -ForegroundColor Green
Write-Host "Complete manifest: $completeManifest" -ForegroundColor Green
