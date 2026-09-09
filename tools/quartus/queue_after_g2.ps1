# queue_after_g2.ps1 -- ONE waiter for everything behind the @g2-prod island fit.
#
# ---------------------------------------------------------------------------
# WHY THIS REPLACES queue_calib_boundary.ps1 RATHER THAN JOINING IT
# ---------------------------------------------------------------------------
# There was already a background job parked on "wait for quartus to go idle".
# Adding a second one would have recreated exactly the race queue_all.ps1 was
# written to remove: two scripts waiting on the same free-resource condition
# both wake when the fit ends and both launch Quartus. One waiter, sequential by
# construction, no inter-job signalling.
#
# Cheapest first, so a kill part-way through still leaves banked rows:
#
#   1. the 32xN calibration boundary   ~75 s   (5 map points)
#   2. zhao_prod_top MapOnly           minutes (never once elaborated)
#   3. zhao_raster_perspuv_pairpipe    minutes (no row at all; prediction filed)
#
# All three are MAP, not FIT. None needs the fitter, and map answers every
# question asked here: DSP inference, memory inference, and whether the thing
# elaborates at all.
$ErrorActionPreference = 'Continue'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

function Wait-Idle {
    while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 60 }
}

Write-Host 'queue_after_g2: waiting for the @g2-prod island fit to finish...'
Wait-Idle

# Perishable: blockpaths/*.setup.rpt for a module is overwritten by the next fit
# of that module, and the census costs seconds.
Write-Host 'queue_after_g2: banking the worst-path census before anything else runs.'
& python tools/quartus/worst_path_index.py

# ---- 1. the calibration boundary (75 s) -----------------------------------
# calibration.json jumps 32x18 (2 DSP) straight to 32x24 (3 DSP). Five widths
# between them have never been measured, and that gap sizes the operand-narrowing
# lever in zhao_project_core: 18 bits is +-2.0 in Q16.16, which caps the longest
# usable lens near a 53 degree vertical FOV. If the real step is at 22,
# coefficients reach +-32.0 and the cap stops constraining the camera.
Write-Host 'queue_after_g2: [1] 32x19..23 calibration points.'
& python tools/budget/gen_calib.py
Wait-Idle
& "$PSScriptRoot\run_calib.ps1" -SkipMeasured 2>&1 |
    Tee-Object -FilePath 'calib-boundary.log' | Select-Object -Last 6

# ---- 2. zhao_prod_top, which has never elaborated -------------------------
# Its row reads failed:quartus_map.exe. Five faults were repaired today -- three
# missing sources, a missing package import, 157 comma-continuation ports at
# width 1 and ten struct inputs at width 1 -- taking Verilator from 12 errors to
# 0 errors and 0 warnings.
#
# THAT IS NOT SYNTHESIZABILITY. This repo has two SystemVerilog forms on record
# that lint with zero diagnostics and fail quartus_map: a bare module-scope
# elaboration `if`, and an implicit generate. So this map is the actual gate, and
# it is cheap -- the previous failure took 33 seconds to appear.
#
# -RowLabel is MANDATORY on a MapOnly (run_block_fit now enforces it): a map row
# carries no ALM and no Fmax, and an unlabelled one would replace the full-fit
# row of the same name.
Write-Host 'queue_after_g2: [2] zhao_prod_top MapOnly -- the real synthesizability gate.'
Wait-Idle
& "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_prod_top' -MapOnly -RowLabel '@map-import-fix' 2>&1 |
    Tee-Object -FilePath 'map-prod-top.log' | Select-Object -Last 8

# ---- 3. the pair-pipe, with a filed prediction ----------------------------
# PRE-REGISTERED in reports/PAIRPIPE-IS-NOT-A-DSP-LEVER-20260909.md: 6 DSP, the
# same as perspuv_svc, because svc has one multiply statement inside a
# `for (ax = 0; ax < 2)` loop (two multipliers) and the pair-pipe has two
# multiply statements (also two). If it comes back LOWER, that reading is wrong
# and I want to know.
Write-Host 'queue_after_g2: [3] zhao_raster_perspuv_pairpipe MapOnly -- scores a filed prediction.'
Wait-Idle
& "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_raster_perspuv_pairpipe' -MapOnly -RowLabel '@map' 2>&1 |
    Tee-Object -FilePath 'map-pairpipe.log' | Select-Object -Last 8

Write-Host 'QUEUE_AFTER_G2 DONE'
