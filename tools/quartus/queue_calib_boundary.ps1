# queue_calib_boundary.ps1 -- locate the 32xN DSP boundary, behind the running fit.
#
# ---------------------------------------------------------------------------
# THE QUESTION
# ---------------------------------------------------------------------------
# calibration.json's asymmetric grid jumps 32x18 (2 DSP) straight to 32x24
# (3 DSP). Five widths in between have never been measured, and that gap decides
# how big a lever operand-narrowing is in zhao_project_core:
#
#   32x32 = 3    32x27 = 3    32x24 = 3    32x18 = 2    27x27 = 1
#
# Narrowing the projector's MATRIX operand to 27 or 24 buys NOTHING; only 18
# moves the cost. 18 bits signed is +-2.0 in Q16.16, and a perspective
# coefficient cot(fov/2)/aspect passes 2.0 at about a 53 degree vertical field of
# view -- so an 18-bit matrix operand caps the longest usable lens.
#
# If the real boundary is 22 rather than 18, coefficients reach +-32.0 and the
# cap stops being a constraint on the camera at all. That is the difference this
# sweep measures, and it is worth about 18 DSP across the two projector cores.
#
# ---------------------------------------------------------------------------
# WHY IT IS QUEUED AND NOT RUN
# ---------------------------------------------------------------------------
# ONE QUARTUS AT A TIME. The @g2-prod island fit is live. Two Quartus processes
# on this machine is how a 55-minute placement died mid-flight when the disk
# filled, and the repo's own convention is Wait-Idle before every stage.
#
# This costs about 15 s per point -- five points, so roughly 75 seconds of
# quartus_map once it starts. It is queued because it must not overlap, not
# because it is expensive.
#
# -SkipMeasured keys on module name against calibration.json, so only the five
# new points run; the twelve existing asymmetric points are not re-measured.
$ErrorActionPreference = 'Continue'
Set-Location 'C:\programmieren\zencrifice\zhaozhou'

Write-Host 'queue_calib_boundary: waiting for the toolchain to go idle...'
while (Get-Process quartus* -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 60 }

Write-Host 'queue_calib_boundary: toolchain idle, regenerating microbenches.'
& python tools/budget/gen_calib.py

Write-Host 'queue_calib_boundary: mapping the unmeasured points.'
& "$PSScriptRoot\run_calib.ps1" -SkipMeasured 2>&1 |
    Tee-Object -FilePath 'calib-boundary.log' | Select-Object -Last 8

Write-Host 'QUEUE_CALIB_BOUNDARY DONE'
