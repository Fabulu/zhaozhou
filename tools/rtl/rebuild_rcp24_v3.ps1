# Rebuild ONLY the rcp24 v3 differential, via verilator directly.
#
# The repository build/ tree is owned by another session this evening (a
# `cmake --preset` into it died on "Error copying file ... Permission denied"),
# and a fresh full configure verilates every target in the project before it
# reaches this one. The fire-test loop needs a rebuild measured in seconds, so
# this drives verilator at the single testbench.
# NOT 'Stop', and NOT `2>&1`. MEASURED 2026-09-06: the first version of this
# script had both, and ALL SIX mutants came back BUILD-FAILED on the text
#
#   verilated.cpp:78:10: warning: 'STDOUT_FILENO' redefined
#
# which is a WARNING from g++ that every build here emits. Windows PowerShell 5.1
# wraps a native executable's stderr in a NativeCommandError ErrorRecord as soon
# as it is redirected inside PowerShell, and `Stop` then terminated the script
# with the compiler still running. Six mutants that were never tested, reported
# as six results. Only the harness's "a mutant that does not compile is NOT a
# caught mutant" rule kept them out of the evidence.
$ErrorActionPreference = 'Continue'
# PATHS ARE DERIVED, NOT TYPED. This script used to hardcode
# C:\programmieren\zencrifice\zhaozhou in five places, which was fine while it
# lived in a run folder that nobody would ever move. It is a committed tool now,
# and a committed tool with one machine's absolute path in it is a tool that
# works exactly once. $PSScriptRoot is tools/rtl, so the repo root is two up.
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$RepoFwd = $Repo -replace '\\', '/'
Set-Location $Repo
. .\tools\env\zhao-env.ps1
$env:PATH = 'C:\Programmieren\dsstuff\mingw64\bin;' + $env:PATH
$obj = Join-Path $Repo 'build-rcp24v3-quick'
$verilator = (Resolve-Path (Join-Path $Repo '..\.tools\oss-cad-suite\bin\verilator_bin.exe')).Path
& $verilator --cc --exe --build -j 4 `
  --prefix Vtb_rcp24_v3_pair --top-module tb_rcp24_v3_pair -Mdir $obj `
  -CFLAGS "-O2 -std=c++17 -I$RepoFwd/tests/harness -I$RepoFwd/reference/include" `
  -o test_rcp24v3 `
  tests/raster/tb_rcp24_v3_pair.sv `
  fpga/rtl/raster/zhao_raster_rcp24.sv fpga/rtl/raster/zhao_raster_rcp24_v3.sv `
  fpga/rtl/raster/zhao_raster_rcp24_mul.sv fpga/rtl/raster/zhao_raster_ticketq.sv `
  fpga/rtl/field/zhao_field_rcp24_rom.sv `
  "$RepoFwd/tests/raster/raster_rcp24_v3_directed.cpp" `
  "$RepoFwd/tests/harness/zhao_sim.cpp" | Out-Null
$rc = $LASTEXITCODE
exit $rc
