@echo off
REM Zhaozhou build environment - verified recipe, 2026-08-14 (oss-cad-suite 20260814:
REM Verilator 5.051 devel, Yosys 0.68+64, SBY 0.68). See PLAN W1 / P1 findings 3.
REM
REM Usage (repo root):   call tools\env\zhao-env.bat
REM
REM Machine rules encoded below (each was reproduced as a hard failure in P1 recon):
REM  - VERILATOR_ROOT must be the share\verilator dir (contains include\ and
REM    verilator-config.cmake), NOT the suite root.
REM  - The devkitPro msys2 cmake is first on PATH by default and is BROKEN with
REM    native g++ - the winlibs dir below is prepended precisely to shadow it.
REM    Use C:\programmieren\dsstuff\mingw64\bin\cmake.exe (4.3.2) + its ninja + g++ 16.1.0.
REM  - oss-cad-suite lib must be on PATH (yosys needs libreadline8.dll there).
REM  - Invoke verilator_bin.exe, never the perl wrapper `verilator`.
REM  - No spaces anywhere in the repo or build path (verilated.mk hard-fails).

set "VERILATOR_ROOT=C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator"
set "PATH=C:\programmieren\zencrifice\.tools\oss-cad-suite\bin;C:\programmieren\zencrifice\.tools\oss-cad-suite\lib;C:\programmieren\dsstuff\mingw64\bin;%PATH%"

echo zhao: VERILATOR_ROOT=%VERILATOR_ROOT%
echo zhao: PATH prepended: oss-cad-suite bin+lib, winlibs cmake/ninja/g++
