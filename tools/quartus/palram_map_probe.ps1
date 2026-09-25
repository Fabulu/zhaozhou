# PALRAM packet probe -- one-change-at-a-time quartus_map of zhao_geom_drawjob.
#
# WHY THIS FILE EXISTS. The ground-contact law: "a probe that does this was
# written once and thrown away, so its numbers are unreproducible -- commit the
# probe." The PALRAM packet's whole deliverable is a CAUSAL answer to "why did
# `logic [383:0] pal_q [256]` not infer M10K", and a causal answer is only worth
# anything if the next person can re-run each step and see the same row.
#
# zhao_geom_drawjob is a true leaf: it instantiates nothing and imports only
# zhao_pkg. So its map cone is exactly two files, which is why a -MapOnly run of
# it costs seconds rather than the 01:54:49 a console fit costs.
#
# USAGE (PowerShell, with tools/env/zhao-env.ps1 sourced, from the repo root):
#   .\tools\quartus\palram_map_probe.ps1 -Label '@palram-base'
#
# Passing -Module overrides the top, so the same flow maps the standalone
# palette probe (tests/probes/zhao_palram_probe.sv) as well as the real block.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Label,
    [string]$Module = 'zhao_geom_drawjob',
    [string[]]$Sources = @(
        'fpga/rtl/common/zhao_pkg.sv',
        'fpga/rtl/geometry/zhao_geom_drawjob.sv'
    ),
    # Passed straight through to run_block_fit.ps1, which validates NAME=VALUE.
    # The probe's arm selector goes here: -TopParameters VARIANT=2. Quartus
    # echoes every one of them into the map report's "Parameter Settings for
    # User Entity Instance" table, so a kept .map.rpt states which arm produced
    # it rather than relying on the row label being typed correctly.
    [string[]]$TopParameters
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner   = Join-Path $PSScriptRoot 'run_block_fit.ps1'

# CAPTURE THE FULL OUTPUT, THEN FILTER THE FILE (ruling R82). Never
# Select-String on the live stream: a failing run is the one you cannot
# re-capture.
$logDir = Join-Path $repoRoot 'reports\synthesis\palram'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$safe = ($Label -replace '[^A-Za-z0-9._-]', '_')
$log  = Join-Path $logDir ("map" + $safe + ".log")

Write-Host ("palram_map_probe: top=" + $Module + " label=" + $Label)
Write-Host ("palram_map_probe: log -> " + $log)

$args2 = @{
    Module       = $Module
    ExtraSources = $Sources
    MapOnly      = $true
    RowLabel     = $Label
}
if ($TopParameters) { $args2['TopParameters'] = $TopParameters }

& $runner @args2 2>&1 | Out-File -Encoding utf8 $log
$rc = $LASTEXITCODE
Write-Host ("palram_map_probe: runner RC=" + $rc)
exit $rc
