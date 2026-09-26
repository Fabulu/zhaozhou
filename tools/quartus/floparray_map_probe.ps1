# FLOPARRAY packet probe -- one-change-at-a-time quartus_map of the two array
# shapes that keep `zhao_forge_assemble` and `zhao_geom_lodstate` in flip-flops.
#
# WHY THIS FILE EXISTS. The ground-contact law: "a probe that does this was
# written once and thrown away, so its numbers are unreproducible -- commit the
# probe." PALRAM and ATTRSETUP both committed theirs. This drives
# `tests/probes/zhao_floparray_probe.sv`, whose header states the four arms.
#
#   VARIANT 0  production verbatim ... reset loop  + comb read   <- CONTROL
#   VARIANT 1  reset loop removed .... no reset    + comb read
#   VARIANT 2  read registered ....... reset loop  + reg read
#   VARIANT 3  both .................. no reset    + reg read
#
# READ ARM 0 FIRST, ALWAYS. It is the positive control and must FAIL to infer.
# If arm 0 infers M10K the probe is not reproducing production and no other
# arm's row means anything -- PALRAM's own doctrine, and the reason its first
# single-cause story was caught.
#
# USAGE (PowerShell, tools/env/zhao-env.ps1 sourced, from the repo root):
#   .\tools\quartus\floparray_map_probe.ps1 -Arm 0
#   .\tools\quartus\floparray_map_probe.ps1 -Arm 3 -Module zhao_floparray_lod_probe
#
# The probe modules are true leaves -- they instantiate nothing and import
# nothing -- so the map cone is one file and a run costs seconds rather than the
# 105 s a real `zhao_forge_assemble` map costs, or the 01:54:49 of a console fit.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateRange(0, 3)][int]$Arm,
    [ValidateSet('zhao_floparray_pos_probe', 'zhao_floparray_lod_probe')]
    [string]$Module = 'zhao_floparray_pos_probe',
    [string[]]$Sources = @('tests/probes/zhao_floparray_probe.sv')
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner   = Join-Path $PSScriptRoot 'run_block_fit.ps1'

# The row label encodes the module AND the arm, because rows merge by name in
# reports/synthesis/zhao_block_fit.json and an unlabelled map row would
# overwrite a full-fit row for the same module.
$tag   = if ($Module -eq 'zhao_floparray_lod_probe') { 'l' } else { 'p' }
$Label = '@probe-' + $tag + $Arm

# CAPTURE THE FULL OUTPUT, THEN FILTER THE FILE (ruling R82).
$logDir = Join-Path $repoRoot 'reports\synthesis\floparray'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$safe = ($Label -replace '[^A-Za-z0-9._-]', '_')
$log  = Join-Path $logDir ("map" + $safe + ".log")

Write-Host ("floparray_map_probe: top=" + $Module + " VARIANT=" + $Arm + " label=" + $Label)
Write-Host ("floparray_map_probe: log -> " + $log)

$args2 = @{
    Module        = $Module
    ExtraSources  = $Sources
    MapOnly       = $true
    RowLabel      = $Label
    TopParameters = @("VARIANT=$Arm")
}

# `*>&1`, NOT `2>&1`. run_block_fit.ps1 reports through Write-Host, which writes
# to the INFORMATION stream (6), not the success stream -- so `2>&1 | Out-File`
# receives an empty pipeline and writes a ZERO-BYTE log while the run itself
# succeeds and prints perfectly to the console. `palram_map_probe.ps1` carries
# this incident (four maps' evidence lost) and THIS PACKET REPRODUCED IT
# ANYWAY on its two baseline runs before reading that comment: both logs came
# back 0 bytes with RC=0. An empty log looks identical to a quiet run, and
# nothing fails to say so. Check the file's SIZE, not the exit code.
& $runner @args2 *>&1 | Out-File -Encoding utf8 $log
$rc = $LASTEXITCODE

if (-not (Test-Path $log) -or ((Get-Item $log).Length -eq 0)) {
    Write-Host "floparray_map_probe: WARNING -- the log is EMPTY. See the note above."
}
Write-Host ("floparray_map_probe: runner RC=" + $rc + " log bytes=" + (Get-Item $log).Length)
exit $rc
