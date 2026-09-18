# run_console_core_smoke.ps1 -- build and run the zhao_console_core wiring
# smoke bench, on THIS machine, reproducibly.
#
# WHY THIS IS A COMMITTED SCRIPT AND NOT A COMMAND SOMEBODY REMEMBERS.
# CLAUDE.md's ground-contact rule generalises: a probe written once and thrown
# away leaves unreproducible numbers. The bench's result ("6 records written
# back, 4 landings, the arena sealed") is evidence about the wiring, and
# evidence nobody can re-run is an assertion.
#
# It is deliberately NOT a CTest entry yet. `tests/CMakeLists.txt` is 12,528
# lines and another agent has work in flight there; registering this belongs
# with the integration owner (plan 13.11), together with the fit target and the
# manifest row. Until then this script is the whole recipe.
#
# ---------------------------------------------------------------------------
# THREE MACHINE FACTS IT ENCODES, each of which cost time to find
# ---------------------------------------------------------------------------
#  1. THERE IS NO `make` ON THIS BOX, and `verilator --binary` shells out to
#     one. So the model is generated with --binary (which still writes the
#     sources and the unit list) and then compiled and linked here by hand.
#  2. `winlibs g++ 16.1.0` SHIPS MISMATCHED HEADERS AND LIBSTDC++: the headers
#     default to _GLIBCXX_USE_CXX11_ABI=1, the library was built with 0. The
#     reproducer is three lines --
#         #include <string>
#         int main(){ std::string a="x"; std::string b=std::move(a); }
#     -- and it fails to link with "undefined reference to
#     std::__cxx11::basic_string<...>::basic_string(basic_string&&)". Hence
#     -D_GLIBCXX_USE_CXX11_ABI=0 below. Anything else linked against these
#     objects needs the same flag.
#  3. COMPILE THE UNIT LIST, NOT EVERY .cpp. Verilator also writes
#     `*_vm_classes_*.cpp` aggregators that #include the individual files;
#     compiling both gives a multiple-definition link failure. The authority is
#     `Vtb_<top>_classes.mk`, which this script parses.
#
# And one PowerShell fact, which produced the most confusing symptom of the
# three: `Set-Location` does NOT change the working directory that native child
# processes inherit. A relative `-o foo.o` therefore lands somewhere else
# entirely while the compile reports success. Every path below is absolute.

[CmdletBinding()]
param(
  [string]$Repo    = $null,
  [string]$BuildIn = $null,
  [switch]$SkipVerilate
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
$gxx = 'C:\programmieren\dsstuff\mingw64\bin'

$env:VERILATOR_ROOT = $vr
$env:PATH = "C:\programmieren\zencrifice\.tools\oss-cad-suite\bin;C:\programmieren\zencrifice\.tools\oss-cad-suite\lib;$gxx;$env:PATH"

if (-not $BuildIn) { $BuildIn = Join-Path $env:TEMP 'zhao_console_core_smoke' }
if (-not (Test-Path $BuildIn)) { New-Item -ItemType Directory -Path $BuildIn | Out-Null }
$bd = (Resolve-Path $BuildIn).Path

$top = 'tb_zhao_console_core_smoke'

# ---------------------------------------------------------------------------
# The source closure is read from design/fit_targets.yml, so the bench and the
# fit measure THE SAME FILES. A second hand-kept copy of a source list is how
# this repository has gone stale before.
# ---------------------------------------------------------------------------
$fitYml = Join-Path $Repo 'design\fit_targets.yml'
$srcRel = @()
$inTarget = $false
foreach ($line in [IO.File]::ReadAllLines($fitYml)) {
  if ($line -match '^\s*- top:\s*(\S+)') { $inTarget = ($Matches[1] -eq 'zhao_console_core'); continue }
  if ($inTarget -and $line -match '^\s*- (fpga/.*\.sv)\s*$') { $srcRel += $Matches[1] }
}
if ($srcRel.Count -lt 50) { throw "fit_targets.yml gave only $($srcRel.Count) sources for zhao_console_core" }

$repoFwd = $Repo.Replace('\', '/')
$srcs = @("$repoFwd/tests/shell/v3_closure_inherited.vlt")
$srcs += ($srcRel | ForEach-Object { "$repoFwd/$_" })
$srcs += "$repoFwd/tests/prod/tb_zhao_console_core_smoke.sv"
Write-Host "closure: $($srcRel.Count) RTL sources from fit_targets.yml"

if (-not $SkipVerilate) {
  # `--cc --exe --main --timing` is deliberately used INSTEAD of `--binary`:
  # --binary would shell out to make, which this box does not have. These
  # flags produce exactly the same sources, main and unit list and stop there.
  Write-Host 'verilating'
  $ErrorActionPreference = 'Continue'
  & $vl --cc --exe --main --timing --timescale 1ns/1ps -Wno-fatal `
        --Mdir ($bd.Replace('\', '/')) --top-module $top --prefix "V$top" @srcs 2>&1 |
    Select-String '%Error' | ForEach-Object { Write-Host $_ }
  $vlrc = $LASTEXITCODE
  $ErrorActionPreference = 'Stop'
  if ($vlrc -ne 0) { throw "verilator returned $vlrc" }
}

$classesMk = Join-Path $bd "V${top}_classes.mk"
if (-not (Test-Path $classesMk)) { throw "verilator produced no $classesMk" }

$units = @()
foreach ($line in [IO.File]::ReadAllLines($classesMk)) {
  $t = $line.Trim().TrimEnd('\').Trim()
  # -cmatch, not -match: PowerShell's -match is case-INSENSITIVE, so the
  # pattern also swallowed the `verilated*` runtime entries and then looked for
  # them in the model directory.
  if ($t -cmatch '^V[A-Za-z0-9_]+$') { $units += $t }
}
$units = $units | Sort-Object -Unique
Write-Host "declared model units: $($units.Count)"

$flags = @('-Os', "-I$bd", "-I$vr/include", "-I$vr/include/vltstd",
  '-DVERILATOR=1', '-DVM_COVERAGE=0', '-DVM_SC=0', '-DVM_TIMING=1', '-DVM_TRACE=0',
  '-DVM_TRACE_FST=0', '-DVM_TRACE_VCD=0', '-DVM_TRACE_SAIF=0', '-DVM_VPI=0',
  '-DVL_TIME_CONTEXT', '-D_GLIBCXX_USE_CXX11_ABI=0',
  '-faligned-new', '-fcf-protection=none', '-fcoroutines', '-w')

Get-ChildItem $bd -Filter '*.o' | ForEach-Object { [IO.File]::Delete($_.FullName) }

$pairs = @()
foreach ($u in $units) { $pairs += , @((Join-Path $bd "$u.cpp"), (Join-Path $bd "$u.o")) }
foreach ($f in @('verilated', 'verilated_dpi', 'verilated_threads', 'verilated_timing')) {
  $pairs += , @("$vr/include/$f.cpp", (Join-Path $bd "$f.o"))
}

$ErrorActionPreference = 'Continue'   # g++ writes warnings to stderr
$fail = 0
foreach ($p in $pairs) {
  if (-not (Test-Path $p[0])) { Write-Host "MISSING SOURCE: $($p[0])"; $fail++; continue }
  & g++ @flags -c $p[0] -o $p[1] 2>&1 | Out-Null
  if (-not (Test-Path $p[1])) { Write-Host "COMPILE FAILED: $($p[0])"; $fail++ }
}
if ($fail -gt 0) { throw "$fail translation unit(s) failed" }
Write-Host "compiled $($pairs.Count) translation units"

$objs = (Get-ChildItem $bd -Filter '*.o' | ForEach-Object { $_.FullName })
$exe = Join-Path $bd 'smoke.exe'
& g++ -o $exe @objs -lpthread 2>&1 | Select-Object -First 8
if (-not (Test-Path $exe)) { throw 'link failed' }
Write-Host "linked $($objs.Count) objects"

Write-Host '--- smoke run ---'
# MINGW64 MUST COME FIRST ON PATH FOR THE RUN. oss-cad-suite's bin/ and lib/
# carry their own libstdc++-6.dll and libwinpthread-1.dll from a different gcc;
# with those ahead of winlibs the binary dies at load with 0xC0000139
# STATUS_ENTRYPOINT_NOT_FOUND, which looks exactly like a bench crash and is
# not one. The verilate step above needs the suite ahead, so the order changes
# here deliberately.
$env:PATH = "$gxx;$env:PATH"
& $exe
$rc = $LASTEXITCODE
Write-Host "SMOKE_RC=$rc"
exit $rc
