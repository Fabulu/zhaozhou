# Zhaozhou build environment — verified recipe, 2026-08-14 (oss-cad-suite 20260814:
# Verilator 5.051 devel, Yosys 0.68+64, SBY 0.68). See PLAN W1 / P1 findings 3.
#
# Usage (repo root):   . ./tools/env/zhao-env.ps1
#
# Machine rules encoded below (each was reproduced as a hard failure in P1 recon):
#   - VERILATOR_ROOT must be the share/verilator dir (contains include/ and
#     verilator-config.cmake), NOT the suite root.
#   - The devkitPro msys2 cmake is first on PATH by default and is BROKEN with
#     native g++ — the winlibs dir below is prepended precisely to shadow it.
#     Use C:\programmieren\dsstuff\mingw64\bin\cmake.exe (4.3.2) + its ninja + g++ 16.1.0.
#   - oss-cad-suite lib/ must be on PATH (yosys needs libreadline8.dll there).
#   - Invoke verilator_bin.exe, never the perl wrapper `verilator`.
#   - No spaces anywhere in the repo or build path (verilated.mk hard-fails).

$env:VERILATOR_ROOT = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
$suiteBin = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin'
$suiteLib = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\lib'
$winlibs  = 'C:\programmieren\dsstuff\mingw64\bin'
$env:PATH = "$suiteBin;$suiteLib;$winlibs;$env:PATH"

Write-Host "zhao: VERILATOR_ROOT=$env:VERILATOR_ROOT" -ForegroundColor Green
Write-Host "zhao: PATH prepended: oss-cad-suite bin+lib, winlibs cmake/ninja/g++" -ForegroundColor Green
