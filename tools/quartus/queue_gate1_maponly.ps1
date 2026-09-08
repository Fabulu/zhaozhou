# queue_gate1_maponly.ps1 — price the migration laboratory, cheaply.
#
# FIT GATE 1 asks what the laboratory costs. The owner's ruling is that fits are
# the scarce resource and only subsystem boundaries earn one, so the question is
# asked the cheap way first: two MapOnly runs of the SAME functional source,
# differing only in `MIGRATION_SHADOWS`.
#
# MapOnly answers exactly the part of gate 1 that matters here — estimated ALMs,
# registers, and whether `sampmeta_m` still infers memory — in minutes rather
# than hours. It CANNOT answer Fmax, and a -MapOnly row deliberately carries no
# ALM or Fmax field for the fitter's own numbers. That limit is the reason this
# is a pre-check and not a replacement for the gate.
#
# THE CONTROLLED PAIR IS THE POINT. Same commit, same source list, same tool,
# one parameter different. That is the only shape in which a delta means the
# laboratory and not the weather — and this repository has twice attributed a
# number to a change when the two sides differed in more than one way (the RCP
# NCTX/TOKW comparison, and the seed spread that withdrew a +6 MHz claim).
$ErrorActionPreference = 'Stop'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

Write-Host 'gate1-maponly: waiting for the toolchain to go idle...'
while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 60 }
Write-Host 'gate1-maponly: toolchain idle.'

# Bank the perishable evidence from whatever ran last before touching anything.
& python tools/quartus/worst_path_index.py

foreach ($cfg in @(
        @{ label = '@g1-lab';  shadows = '1' },
        @{ label = '@g1-prod'; shadows = '0' })) {
    Write-Host ("gate1-maponly: MIGRATION_SHADOWS=" + $cfg.shadows + " -> " + $cfg.label)
    & "$PSScriptRoot\run_block_fit.ps1" `
        -Module 'zhao_texture_island_v3_top' `
        -RowLabel $cfg.label `
        -MapOnly `
        -TopParameters @("MIGRATION_SHADOWS=" + $cfg.shadows) 2>&1 |
        Tee-Object -FilePath ("map-g1-" + $cfg.shadows + ".log") | Select-Object -Last 4
}
Write-Host 'GATE1MAPONLY DONE'
