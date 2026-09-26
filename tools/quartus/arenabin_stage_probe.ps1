# ARENAINFER packet probe -- one-change-at-a-time quartus_map of the
# `zhao_geom_arenabin` staging banks.
#
# WHY THIS FILE EXISTS. The ground-contact law: "a probe that does this was
# written once and thrown away, so its numbers are unreproducible -- commit the
# probe." The ARENAINFER packet's deliverable is a CAUSAL answer to "why did
# fourteen 576 x 18 banks declared inside a generate not infer", and a causal
# answer is only worth anything if the next person can re-run each step and see
# the same row.
#
# Modelled on tools/quartus/palram_map_probe.ps1, including the `*>&1` lesson
# in it: run_block_fit.ps1 reports through Write-Host, which writes to the
# INFORMATION stream, so `2>&1 | Out-File` captures a ZERO-BYTE log while the
# run succeeds.
#
# THE DEVICE. `-Device` is deliberately NOT passed. run_block_fit.ps1 writes it
# into the QSF UNVALIDATED -- ARENACOMPOSE produced a row whose `measuredDevice`
# was a FILE PATH -- and with it absent the shell_fit QSF's own 5CSEBA6U23I7
# stands, which is the shipping part these rows must be taken on. Every row is
# still stamped `measuredDevice` by the runner; CHECK IT on the row you just
# wrote.
#
# USAGE (PowerShell, with tools/env/zhao-env.ps1 sourced, from the repo root):
#   .\tools\quartus\arenabin_stage_probe.ps1 -Label '@stage-v0' `
#        -TopParameters VARIANT=0,STAGE_IDS=1,STG_W=4
#
# STG_W is derived from STAGE_IDS in the module header, but Quartus applies
# -TopParameters as overrides of the DECLARED defaults rather than re-deriving
# them, so a run that changes STAGE_IDS must state STG_W too. Getting that wrong
# is silent: the banks still elaborate and the widths merely stop matching.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Label,
    [string]$Module = 'zhao_arenabin_stage_probe',
    # `zhao_dc_sdp_ram.sv` is here for arm 7 only. It is harmless in every
    # other arm -- nothing instantiates it, so Quartus elaborates nothing from
    # it -- and having ONE source list keeps the arms comparable.
    [string[]]$Sources = @(
        'fpga/rtl/common/zhao_dc_sdp_ram.sv',
        'tests/probes/zhao_arenabin_stage_probe.sv'
    ),
    [string[]]$TopParameters
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner   = Join-Path $PSScriptRoot 'run_block_fit.ps1'

$logDir = Join-Path $repoRoot 'reports\synthesis\arenabin'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$safe = ($Label -replace '[^A-Za-z0-9._-]', '_')
$log  = Join-Path $logDir ("map" + $safe + ".log")

Write-Host ("arenabin_stage_probe: top=" + $Module + " label=" + $Label)
Write-Host ("arenabin_stage_probe: log -> " + $log)

# A SPLAT, NOT A STRING. Splitting -ExtraSources through `powershell -File`
# breaks the array apart; the brief names this trap explicitly.
$args2 = @{
    Module       = $Module
    ExtraSources = $Sources
    MapOnly      = $true
    RowLabel     = $Label
}
if ($TopParameters) { $args2['TopParameters'] = $TopParameters }

& $runner @args2 *>&1 | Out-File -Encoding utf8 $log
$rc = $LASTEXITCODE
Write-Host ("arenabin_stage_probe: runner RC=" + $rc)
exit $rc
