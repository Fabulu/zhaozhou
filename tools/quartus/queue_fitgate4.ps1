# queue_fitgate4.ps1 — FIT GATE 4: the matched RCP pair, after the island fits.
#
# Owner ruling 2026-09-08: "halve the DSPs even if it costs." The island is in
# breach of its DSP budget (17 against 14) and swapping rcp24_svc (6 DSP) for
# rcp24_v3 (3 DSP) lands it at exactly 14, so this pair is authorised.
#
# WHY IT WAITS. The machine is at 100% load with two island fits in placement.
# Four concurrent Quartus fits slow all four and this repository has already
# lost a fit to a full disk. This waits for the toolchain to go idle, banks the
# perishable evidence, then runs the pair.
#
# WHY IT BANKS FIRST. `reports/synthesis/blockpaths/<module>.setup.rpt` is
# OVERWRITTEN by the next fit of the same module. These are different modules,
# so nothing of the island's is at risk from them -- but the census costs
# seconds, the evidence cannot be recovered afterwards, and the habit is the
# point.
#
# BOTH BLOCKS AT THE SAME PROFILE. NCTX=12, TOKW=14 for each. Twelve because it
# is the measured cheapest profile at which V3 meets its own 4.6 clk/recip
# criterion; the same for both because the standing rows differ in NCTX *and*
# TOKW and comparing them was the mistake this gate exists to correct.
#
# The -TopParameters array form is explicit. Passing 'NCTX=12,TOKW=14' as ONE
# quoted string is what killed @island-profile in 38 seconds; run_block_fit.ps1
# now refuses that, but writing it correctly here is better than relying on the
# guard.
$ErrorActionPreference = 'Stop'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

Write-Host 'fitgate4: waiting for the toolchain to go idle...'
while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 60 }
Write-Host 'fitgate4: toolchain idle.'

# Bank the census before anything else touches the setup reports.
& python tools/quartus/worst_path_index.py
Write-Host 'fitgate4: worst-path census banked.'

foreach ($mod in @('zhao_raster_rcp24_v3', 'zhao_raster_rcp24_svc')) {
    Write-Host ("fitgate4: fitting " + $mod + " at NCTX=12 TOKW=14")
    & powershell -NoProfile -File tools\quartus\run_block_fit.ps1 `
        -Module $mod `
        -RowLabel '@g4-nctx12' `
        -TopParameters @('NCTX=12', 'TOKW=14') 2>&1 |
        Tee-Object -FilePath ("fit-g4-" + $mod + ".log") | Select-Object -Last 4
}
Write-Host 'FITGATE4 DONE'
