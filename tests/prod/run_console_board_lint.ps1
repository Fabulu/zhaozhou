# run_console_board_lint.ps1 -- lint `zhao_console_board`, THE TOP THAT COULD BE
# PROGRAMMED ONTO HARDWARE, over its real closure.
#
# WHY THIS SCRIPT EXISTS AT ALL
# -----------------------------
# Until 2026-09-19 the board's lint named three files -- the board, SYS.PLL and
# SYS.RESET -- because that was genuinely the whole closure: the board did not
# instantiate `zhao_console_core`. It does now, so the closure is the console's
# 170-odd sources plus those three, and a hand-typed list of them would be a
# SECOND copy of a source list this repository already keeps in one place.
#
# THE CLOSURE IS READ FROM design/fit_targets.yml, exactly as
# `run_console_core_smoke.ps1` reads it, so the board lint, the smoke bench and
# the fit all measure THE SAME FILES. That is the sharing this script buys: no
# second list, and a source added to the console is in the board's lint the
# moment it is in the console's fit.
#
# WHAT IT PROVES AND WHAT IT DOES NOT
# -----------------------------------
# It proves the board ELABORATES: that every one of the 1,038 connections the
# generator wrote binds to a port that exists, at the width it exists at, and
# that nothing is left unconnected. A dropped or misspelled port is a
# `PINMISSING`/`MODMISSING` here in seconds instead of hours into a fit.
#
# It proves NOTHING about synthesizability. CLAUDE.md is explicit: a block that
# has never been through `quartus_map` has not been shown to be synthesizable,
# however clean its lint, and two SystemVerilog forms are on record that pass
# `verilator --lint-only` with zero diagnostics and fail `quartus_map`. This
# file has never been mapped. `tools/quartus/check_quartus17_syntax.py` is the
# cheap half of that gap and runs separately.
#
# -Waived uses the same `.vlt` the console's own lint uses. The waivers belong
# to the console's sources, not to the board -- the board adds none of its own,
# which is a claim this script makes checkable by running both ways.

[CmdletBinding()]
param(
  [string]$Repo = $null,
  [switch]$NoWaivers
)

$ErrorActionPreference = 'Stop'

# `$PSScriptRoot` is empty inside a param() default under Windows PowerShell
# 5.1 in some invocation modes, so the default is resolved here instead.
if (-not $Repo) {
  $here = Split-Path -Parent $MyInvocation.MyCommand.Path
  $Repo = (Resolve-Path (Join-Path $here '..\..')).Path
}

$vr = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
$vl = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe'
$env:VERILATOR_ROOT = $vr
# `verilator_bin.exe` links against DLLs in the suite's own bin and lib. Without
# them on PATH it dies with 0xC0000135 (STATUS_DLL_NOT_FOUND), which surfaces as
# an exit code of -1073741515 and NO DIAGNOSTIC AT ALL -- a silence that a lint
# wrapper reading only "did it print anything" would report as clean. The exit
# code is checked before the output for exactly that reason.
$env:PATH = "C:\programmieren\zencrifice\.tools\oss-cad-suite\bin;C:\programmieren\zencrifice\.tools\oss-cad-suite\lib;$env:PATH"

# ---------------------------------------------------------------------------
# The console's own source list, from the one place it is written down.
# ---------------------------------------------------------------------------
$fitYml = Join-Path $Repo 'design\fit_targets.yml'
$srcRel = @()
$inTarget = $false
foreach ($line in [IO.File]::ReadAllLines($fitYml)) {
  if ($line -match '^\s*- top:\s*(\S+)') { $inTarget = ($Matches[1] -eq 'zhao_console_core'); continue }
  if ($inTarget -and $line -match '^\s*- (fpga/.*\.sv)\s*$') { $srcRel += $Matches[1] }
}
# A closure that came back small is the broken-instrument failure this
# repository has a chapter about: it would lint a handful of files, report
# clean, and say nothing whatever about the board. Refuse instead.
if ($srcRel.Count -lt 50) {
  throw "fit_targets.yml gave only $($srcRel.Count) sources for zhao_console_core -- refusing to report a clean lint over a closure that small"
}

$repoFwd = $Repo.Replace('\', '/')
$srcs = @()
if (-not $NoWaivers) { $srcs += "$repoFwd/tests/shell/v3_closure_inherited.vlt" }
$srcs += ($srcRel | ForEach-Object { "$repoFwd/$_" })
$srcs += "$repoFwd/fpga/rtl/sys/zhao_sys_pll.sv"
$srcs += "$repoFwd/fpga/rtl/sys/zhao_sys_reset.sv"
$srcs += "$repoFwd/fpga/rtl/prod/zhao_console_board.sv"

Write-Host "closure: $($srcRel.Count) console sources from fit_targets.yml + SYS.PLL + SYS.RESET + the board"

$ErrorActionPreference = 'Continue'
$out = & $vl --lint-only -Wall --top-module zhao_console_board @srcs 2>&1
$rc = $LASTEXITCODE
$ErrorActionPreference = 'Stop'

$out | ForEach-Object { Write-Host $_ }
if ($rc -ne 0) { throw "verilator --lint-only returned $rc for zhao_console_board" }

# SILENT means NO DIAGNOSTIC, not "no output". This closure makes Verilator
# print its three-line verilation report on success, and a check of "did it
# print anything" would fail on that forever -- which is how a gate becomes one
# people learn to skip. Match the diagnostic markers instead, and match them on
# the line rather than the exit code, because `-Wno-fatal` elsewhere in this
# tree makes RC 0 compatible with a page of warnings.
$diag = $out | Where-Object { "$_" -match '%(Warning|Error)' }
if ($diag) { throw "zhao_console_board lint was not SILENT -- $($diag.Count) diagnostic(s) above" }

Write-Host 'lint_zhao_console_board: SILENT, RC 0 -- the board elaborates the console.'
exit 0
