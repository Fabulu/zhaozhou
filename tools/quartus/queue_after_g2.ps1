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
# `*>&1`, NOT `2>&1`, ON EVERY TEE BELOW.
#
# Write-Host writes to the information stream and never enters an in-process
# pipeline, so `& script.ps1 2>&1 | Tee-Object -FilePath log` records everything
# EXCEPT it. run_block_fit emits its whole provenance trail that way -- preflight,
# snapshot, provenance guard, source digest -- so these logs were silently missing
# the four lines that make them receipts rather than transcripts.
#
# The older logs in the repo root DO carry those lines, which is what made this
# hard to see: fit-d0fixed.log has all four. Those were written by OS-level
# redirection of the whole powershell process, where Write-Host lands on the
# redirected console. Same-looking file, different capture, different contents.
#
# Verified on the REAL pattern, because it differs from the obvious test: a
# script BLOCK and a script FILE behave the same here, but `2>&1` loses
# HOST-FROM-FILE and `*>&1` keeps it. The first demonstration used a block and
# proved the wrong half.
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
& "$PSScriptRoot\run_calib.ps1" -SkipMeasured *>&1 |
    Tee-Object -FilePath 'calib-boundary.log' | Select-Object -Last 6

# DID -SkipMeasured ACTUALLY SKIP? Verify, do not assume.
#
# Its destination path had a literal 0x08 backspace where `\b` was written, so it
# looked for a file that cannot exist, skipped nothing, and re-measured all 123
# points -- roughly two hours -- every time it ran. It ANNOUNCED this on its third
# line ("0 of 123 already ok") and I launched the job without reading the first
# lines of its output.
#
# So the claim is checked rather than trusted. A run that skips nothing means the
# flag is broken again, and finding that out from a log line beats finding it out
# from two hours of wall clock.
$skipLine = Select-String -Path 'calib-boundary.log' -Pattern 'SkipMeasured: (\d+) of (\d+)' |
    Select-Object -First 1
if ($skipLine) {
    $already = [int]$skipLine.Matches[0].Groups[1].Value
    $of      = [int]$skipLine.Matches[0].Groups[2].Value
    Write-Host ("queue_after_g2: -SkipMeasured skipped {0} of {1}." -f $already, $of)
    if ($already -eq 0 -and $of -gt 10) {
        Write-Host 'queue_after_g2: WARNING -- it skipped NOTHING. The destination'
        Write-Host '  path is broken again (check for a stray control byte with'
        Write-Host '  tools/maintenance/no_control_bytes.py). Later stages continue.'
    }
} else {
    Write-Host 'queue_after_g2: could not find the -SkipMeasured line in the log.'
}

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
# try/catch, because $ErrorActionPreference = 'Continue' does NOT survive a
# `throw`. run_block_fit's preflight throws by design, and on 2026-09-09 one such
# throw -- the module-declaration matcher rejecting a legal header-import form --
# terminated this whole script and took stage 3 with it. The header above says
# every step is independently useful; that was true only until the first throw.
try {
    & "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_prod_top' -MapOnly -RowLabel '@map-import-fix' *>&1 |
        Tee-Object -FilePath 'map-prod-top.log' | Select-Object -Last 8
} catch {
    Write-Host ('queue_after_g2: [2] FAILED -- ' + $_.Exception.Message)
    Write-Host 'queue_after_g2: continuing to stage 3 anyway.'
}

# ---- 3. the pair-pipe, with a filed prediction ----------------------------
# PRE-REGISTERED in reports/PAIRPIPE-IS-NOT-A-DSP-LEVER-20260909.md: 6 DSP, the
# same as perspuv_svc, because svc has one multiply statement inside a
# `for (ax = 0; ax < 2)` loop (two multipliers) and the pair-pipe has two
# multiply statements (also two). If it comes back LOWER, that reading is wrong
# and I want to know.
Write-Host 'queue_after_g2: [3] zhao_raster_perspuv_pairpipe MapOnly -- scores a filed prediction.'
Wait-Idle
try {
    & "$PSScriptRoot\run_block_fit.ps1" -Module 'zhao_raster_perspuv_pairpipe' -MapOnly -RowLabel '@map' *>&1 |
        Tee-Object -FilePath 'map-pairpipe.log' | Select-Object -Last 8
} catch {
    Write-Host ('queue_after_g2: [3] FAILED -- ' + $_.Exception.Message)
}

Write-Host 'QUEUE_AFTER_G2 DONE'
