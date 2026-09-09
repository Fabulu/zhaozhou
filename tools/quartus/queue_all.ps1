# queue_all.ps1 — ONE wrapper, SHORT work first.
#
# ---------------------------------------------------------------------------
# WHY THIS REPLACES THREE SEPARATE QUEUE SCRIPTS
# ---------------------------------------------------------------------------
# Twice on 2026-09-08/09 an external stop killed every background job and took
# their Quartus children with them. It cost ~3.25 hours the first time (two island
# fits at ~195 and ~174 minutes) and ~1.7 hours the second (the rcp24_svc matched
# row at 102 of an expected ~104 minutes — killed at roughly 98% done, writing no
# row at all).
#
# Two conclusions, and neither is "try harder not to be killed":
#
# 1. **A long fit is a bet against the next kill; a short measurement is not.**
#    So the order is now cheapest-first. Every MapOnly here finishes in minutes and
#    banks a row before anything long is attempted. The long fit goes LAST, where
#    losing it costs only itself.
#
# 2. **Fewer wrappers is fewer things to lose.** Three scripts each waiting on a
#    different condition was also three processes to kill and a race to get wrong
#    (which happened: two of them waited on the same free-resource condition).
#    One script, sequential by construction, no inter-job signalling at all.
#
# Every step is independently useful, so a kill part-way through still leaves
# banked rows rather than nothing.
$ErrorActionPreference = 'Continue'   # one failing step must not abandon the rest
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

function Wait-Idle {
    while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 30 }
}

Write-Host 'queue_all: waiting for the toolchain to go idle...'
Wait-Idle

# Perishable first: blockpaths/*.setup.rpt is overwritten by the next fit of the
# same module, and the census costs seconds.
& python tools/quartus/worst_path_index.py
Write-Host 'queue_all: worst-path census banked.'

# ---- STAGE 1: gate 1's controlled pair (minutes) --------------------------
# Same functional source, one parameter different. Prices the migration
# laboratory and unblocks Packet 2.
foreach ($cfg in @(@{ label = '@g1-lab'; shadows = '1' },
                   @{ label = '@g1-prod'; shadows = '0' })) {
    Write-Host ("queue_all: [1] island MapOnly MIGRATION_SHADOWS=" + $cfg.shadows)
    Wait-Idle
    & "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_texture_island_v3_top' `
        -RowLabel $cfg.label -MapOnly `
        -TopParameters @("MIGRATION_SHADOWS=" + $cfg.shadows) 2>&1 |
        Tee-Object -FilePath ("map-g1-" + $cfg.shadows + ".log") | Select-Object -Last 3
}

# ---- STAGE 2: the unmeasured and the unexplained (minutes each) -----------
# metajoin has no fit row at all despite being instantiated by the island;
# early_desc and uv_join test §D's memory-geometry claims; the two combine
# blocks test lever 4's multstyle hypothesis and produce the v1 row that
# prod_manifest's "delete when v1 is measured" has been waiting for.
foreach ($mod in @('zhao_texture_metajoin',
                   'zhao_texture_early_desc',
                   'zhao_texture_uv_join',
                   'zhao_texture_combine',
                   'zhao_texture_material_combine_v1')) {
    Write-Host ("queue_all: [2] MapOnly " + $mod)
    Wait-Idle
    & "$PSScriptRoot\run_block_fit.ps1" -Module $mod -MapOnly 2>&1 |
        Tee-Object -FilePath ("map-nb-" + $mod + ".log") | Select-Object -Last 3
}

# ---- STAGE 3: the long one, LAST -----------------------------------------
# gate 4's matched svc row. Without it there is no like-for-like comparison for
# rcp24_v3@g4-nctx12, and putting that row beside the standing NCTX=8/TOKW=8 svc
# row would be exactly the mismatched comparison the owner memo retracts.
Write-Host 'queue_all: [3] rcp24_svc @ NCTX=12 TOKW=14 (the long one)'
Wait-Idle
& "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_raster_rcp24_svc' `
    -RowLabel '@g4-nctx12' `
    -TopParameters @('NCTX=12', 'TOKW=14') 2>&1 |
    Tee-Object -FilePath 'fit-g4-svc.log' | Select-Object -Last 4

Write-Host 'QUEUE_ALL DONE'
