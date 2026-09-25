# ATTRSETUP packet probe -- one-change-at-a-time quartus_map of the 45-DSP block.
#
# WHY THIS FILE EXISTS. The ground-contact law: "a probe that does this was
# written once and thrown away, so its numbers are unreproducible -- commit the
# probe." The ATTRSETUP packet's deliverable is a CAUSAL answer to "why does
# zhao_geom_attrsetup cost 45 DSP blocks, 40% of the shipping device, out of 164
# lines", and a causal answer is only worth anything if the next person can
# re-run each step and see the same row.
#
# zhao_geom_attrsetup is a true leaf: it instantiates nothing and imports
# nothing. Its map cone is exactly one file, which is why a -MapOnly run costs
# ~19 s against the 01:54:49 a console fit costs. That is what makes a leaf map
# an honest stand-in on this question -- and the census established the other
# half of that argument: the standalone DSP counts match the composed entity
# table EXACTLY, so a DSP saved here is a DSP saved in the console, one for one.
#
# USAGE (PowerShell, with tools/env/zhao-env.ps1 sourced, from the repo root):
#
#   # the production block
#   .\tools\quartus\attrsetup_map_probe.ps1 -Label '@gz-base'
#
#   # one probe arm
#   .\tools\quartus\attrsetup_map_probe.ps1 -Label '@probe-m0' `
#       -Module zhao_attrsetup_mul_probe `
#       -Sources 'tests/probes/zhao_attrsetup_mul_probe.sv' `
#       -TopParameters VARIANT=0
#
# -Label must start with '@' or '-'; run_block_fit.ps1 refuses anything else,
# because the row name is "$Module$Label" glued and an unprefixed label produces
# a row nobody recognises.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Label,
    [string]$Module = 'zhao_geom_attrsetup',
    [string[]]$Sources = @(
        'fpga/rtl/geometry/zhao_geom_attrsetup.sv'
    ),
    # Passed straight through to run_block_fit.ps1, which validates NAME=VALUE.
    # The probe's arm selector goes here: -TopParameters VARIANT=2. Quartus
    # echoes every one into the map report's "Parameter Settings for User Entity
    # Instance" table, so a kept .map.rpt STATES which arm produced it rather
    # than relying on the row label having been typed correctly.
    [string[]]$TopParameters
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner   = Join-Path $PSScriptRoot 'run_block_fit.ps1'

# CAPTURE THE FULL OUTPUT, THEN FILTER THE FILE (ruling R82). Never
# Select-String on the live stream: a failing run is the one you cannot
# re-capture.
$logDir = Join-Path $repoRoot 'reports\synthesis\attrsetup'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$safe = ($Label -replace '[^A-Za-z0-9._-]', '_')
$log  = Join-Path $logDir ("map" + $safe + ".log")

Write-Host ("attrsetup_map_probe: top=" + $Module + " label=" + $Label)
if ($TopParameters) {
    Write-Host ("attrsetup_map_probe: params=" + ($TopParameters -join ' '))
}
Write-Host ("attrsetup_map_probe: log -> " + $log)

$args2 = @{
    Module       = $Module
    ExtraSources = $Sources
    MapOnly      = $true
    RowLabel     = $Label
}
if ($TopParameters) { $args2['TopParameters'] = $TopParameters }

# `*>&1`, NOT `2>&1` -- inherited from tools/quartus/palram_map_probe.ps1, which
# paid four zero-byte logs to learn it. run_block_fit.ps1 reports through
# Write-Host, i.e. the INFORMATION stream (6), which `2>&1` does not merge, so
# Out-File receives an empty pipeline and writes a 0-byte file while the run
# itself succeeds. "I redirected the output" is not "I captured the output", and
# an empty log looks identical to a quiet run. Check the log's SIZE.
& $runner @args2 *>&1 | Out-File -Encoding utf8 $log
$rc = $LASTEXITCODE
Write-Host ("attrsetup_map_probe: runner RC=" + $rc + "  log bytes=" + (Get-Item $log).Length)
exit $rc
