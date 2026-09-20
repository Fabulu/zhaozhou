# run_r126_guard.ps1 -- the INVERTED-POLARITY driver for R126's elaboration
# guard in `fpga/rtl/field/zhao_field_host_v2.sv`.
#
# THIS SCRIPT PASSES WHEN THE GUARD KILLS THE BINARY.
#
# Owner ruling R126 assigns the guard to H1 and states the two facts that make
# an ordinary test useless here:
#
#   * Quartus 17.0 rejects a bare module-scope `if`, so the guard lives inside
#     `initial begin ... end`;
#   * `verilator --lint-only` DOES NOT RUN `initial` blocks, so a clean lint is
#     "no evidence whatever" about it.
#
# `tests/CMakeLists.txt` builds `test_field_host_v2_r126` from the same host
# sources with `-GOUT_LANES=5` while the generated schema fixes
# `ZFH_WINDOW_MASK_BITS = 7`. That is exactly the disagreement R126 describes.
#
# WHY THIS DOES NOT SIMPLY CHECK FOR A NON-ZERO EXIT CODE. A process dies for
# many reasons -- a segfault, a missing runtime DLL, an unrelated assertion, a
# kill from another lane sharing this machine. A control that accepts ANY death
# as proof cannot distinguish the thing it was built to prove, and it would go
# on passing after somebody deleted the guard, which is the flattering
# direction. So the fatal text must name BOTH quantities by name.

param(
    [string]$Exe = ""
)

$ErrorActionPreference = "Continue"

if ($Exe -eq "") {
    $root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $candidates = @(
        (Join-Path $root "build\tests\test_field_host_v2_r126.exe"),
        (Join-Path $root "build\tests\Release\test_field_host_v2_r126.exe"),
        (Join-Path $root "build\tests\Debug\test_field_host_v2_r126.exe")
    )
    foreach ($c in $candidates) { if (Test-Path $c) { $Exe = $c; break } }
}

if (($Exe -eq "") -or (-not (Test-Path $Exe))) {
    Write-Output "R126 DRIVER: FAIL -- could not find test_field_host_v2_r126.exe."
    Write-Output "R126 DRIVER: a control that cannot find its subject has proved nothing."
    exit 1
}

# CAPTURE THE FULL OUTPUT TO A FILE, THEN FILTER THE FILE (ruling R82).
# Filtering a live stream costs nothing when the run passes and costs the whole
# diagnosis when it fails -- and a failing run is the one that cannot be
# re-captured.
$log = Join-Path $env:TEMP ("gz-fieldh1-r126-" + [System.Guid]::NewGuid().ToString("N") + ".log")
& $Exe *>&1 | Out-File -Encoding utf8 $log
$rc = $LASTEXITCODE

$text = ""
if (Test-Path $log) { $text = [IO.File]::ReadAllText($log) }

Write-Output "R126 DRIVER: subject      = $Exe"
Write-Output "R126 DRIVER: exit code    = $rc"
Write-Output "R126 DRIVER: captured log = $log"
Write-Output "---------------- captured output ----------------"
Write-Output $text
Write-Output "-------------------------------------------------"

$didNotFire = $text.Contains("R126-GUARD-DID-NOT-FIRE")
$namesLanes = $text.Contains("OUT_LANES")
$namesBits  = $text.Contains("ZFH_WINDOW_MASK_BITS")

if ($didNotFire) {
    Write-Output "R126 DRIVER: FAIL -- the host ELABORATED at OUT_LANES=5 while the"
    Write-Output "R126 DRIVER: schema fixes 7. The guard is absent or unreachable, and"
    Write-Output "R126 DRIVER: the two quantities R126 names can now disagree silently."
    exit 1
}

if ($rc -eq 0) {
    Write-Output "R126 DRIVER: FAIL -- the binary exited 0. Nothing stopped a wrong width."
    exit 1
}

if (-not ($namesLanes -and $namesBits)) {
    Write-Output "R126 DRIVER: FAIL -- the process died (rc=$rc) but its output does not"
    Write-Output "R126 DRIVER: name both OUT_LANES and ZFH_WINDOW_MASK_BITS. Something"
    Write-Output "R126 DRIVER: killed it, and it was not this guard. A control that"
    Write-Output "R126 DRIVER: accepts any death proves nothing about the one it wants."
    Write-Output "R126 DRIVER:   names OUT_LANES            = $namesLanes"
    Write-Output "R126 DRIVER:   names ZFH_WINDOW_MASK_BITS = $namesBits"
    exit 1
}

Write-Output "R126 DRIVER: PASS -- the guard FIRED, and it fired for its own reason:"
Write-Output "R126 DRIVER: the fatal names OUT_LANES and ZFH_WINDOW_MASK_BITS, and the"
Write-Output "R126 DRIVER: 'did not fire' line was never reached. R126 is discharged by"
Write-Output "R126 DRIVER: a firing, not by a clean lint -- which would have said nothing."
Remove-Item -Force $log -ErrorAction SilentlyContinue
exit 0
