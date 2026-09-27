# run_terrain_matpub_directed.ps1 -- build and run TERRAIN.COMPOSED_MATERIAL's
# directed test on THIS machine, without a CMake tree.
#
# WHY THIS EXISTS. `tests/CMakeLists.txt` registers this test and that
# registration is the one that ships; but configuring a CMake tree in a fresh
# worktree verilates the WHOLE suite, which is hours, and both of this entry's
# previous packets ended up quoting their directed tests as "not run" for
# exactly that reason. A block whose test has never been executed is a block
# with a claim attached, so this script is the short path to executing it.
#
# It encodes the same three machine facts `tests/prod/run_console_core_smoke.ps1`
# encodes, because they are properties of the box and not of that bench:
#   1. there is no `make` here, so `--binary` cannot be used; `--cc --exe`
#      writes the same sources and the unit list and stops;
#   2. winlibs g++ ships headers and libstdc++ built with different
#      _GLIBCXX_USE_CXX11_ABI, hence the explicit -D;
#   3. the unit list to compile is `V<top>_classes.mk`, not every .cpp -- the
#      aggregators would give multiple-definition link errors.
# And the PATH order flips between verilate and run: oss-cad-suite's own
# libstdc++ ahead of winlibs makes the exe die at load with 0xC0000139, which
# reads exactly like a bench crash and is not one.

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $here '..\..')).Path
$R = $Repo.Replace('\', '/')

$vr  = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
$vl  = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe'
$gxx = 'C:\programmieren\dsstuff\mingw64\bin'
$env:VERILATOR_ROOT = $vr
$env:PATH = "C:\programmieren\zencrifice\.tools\oss-cad-suite\bin;C:\programmieren\zencrifice\.tools\oss-cad-suite\lib;$gxx;$env:PATH"

$top = 'tb_terrain_matpub'
# PER CHECKOUT, for the reason the smoke script gives: two worktrees verilating
# into one directory fail each other's link with an undefined-reference that
# reads exactly like a partition bug in your own change.
$sha = [System.Security.Cryptography.SHA1]::Create()
$key = ($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Repo.ToLowerInvariant())) |
        Select-Object -First 4 | ForEach-Object { $_.ToString('x2') }) -join ''
$bd = Join-Path $env:TEMP "zhao_${top}_$key"
if (-not (Test-Path $bd)) { New-Item -ItemType Directory -Path $bd | Out-Null }
$bd = (Resolve-Path $bd).Path
Write-Host "build dir: $bd"

$srcs = @(
  "$R/fpga/rtl/generated/zhao_abi_pkg.sv",
  "$R/fpga/rtl/common/zhao_pkg.sv",
  "$R/fpga/rtl/common/zhao_dc_sdp_ram.sv",
  "$R/fpga/rtl/terrain/zhao_terrain_matpub.sv",
  "$R/tests/terrain/tb_terrain_matpub.sv"
)

Write-Host 'verilating'
$ErrorActionPreference = 'Continue'
& $vl --cc --exe --timing --timescale 1ns/1ps -Wno-fatal `
      --Mdir ($bd.Replace('\','/')) --top-module $top --prefix "V$top" `
      @srcs "$R/tests/terrain/terrain_matpub_directed.cpp" 2>&1 |
  Select-String '%Error' | ForEach-Object { Write-Host $_ }
$vlrc = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($vlrc -ne 0) { throw "verilator returned $vlrc" }

$classesMk = Join-Path $bd "V${top}_classes.mk"
if (-not (Test-Path $classesMk)) { throw "verilator produced no $classesMk" }
$units = @()
foreach ($line in [IO.File]::ReadAllLines($classesMk)) {
  $t = $line.Trim().TrimEnd('\').Trim()
  # -cmatch, not -match: -match is case-INSENSITIVE and would also swallow the
  # `verilated*` runtime entries, then look for them in the model directory.
  if ($t -cmatch '^V[A-Za-z0-9_]+$') { $units += $t }
}
$units = $units | Sort-Object -Unique
Write-Host "declared model units: $($units.Count)"

$flags = @('-Os', "-I$bd", "-I$vr/include", "-I$vr/include/vltstd",
  "-I$Repo/tests/harness", "-I$Repo/tests/terrain",
  '-DVERILATOR=1','-DVM_COVERAGE=0','-DVM_SC=0','-DVM_TIMING=1','-DVM_TRACE=0',
  '-DVM_TRACE_FST=0','-DVM_TRACE_VCD=0','-DVM_TRACE_SAIF=0','-DVM_VPI=0',
  '-DVL_TIME_CONTEXT','-D_GLIBCXX_USE_CXX11_ABI=0',
  '-faligned-new','-fcf-protection=none','-fcoroutines','-w')

Get-ChildItem $bd -Filter '*.o' | ForEach-Object { [IO.File]::Delete($_.FullName) }
$pairs = @()
foreach ($u in $units) { $pairs += , @((Join-Path $bd "$u.cpp"), (Join-Path $bd "$u.o")) }
foreach ($f in @('verilated','verilated_dpi','verilated_threads','verilated_timing')) {
  $pairs += , @("$vr/include/$f.cpp", (Join-Path $bd "$f.o"))
}
$pairs += , @("$Repo/tests/terrain/terrain_matpub_directed.cpp", (Join-Path $bd 'matpub_directed.o'))

$ErrorActionPreference = 'Continue'
$fail = 0
foreach ($p in $pairs) {
  if (-not (Test-Path $p[0])) { Write-Host "MISSING SOURCE: $($p[0])"; $fail++; continue }
  # The compiler's output is KEPT. A failure report that names the file and
  # withholds the error is worse than a crash, because it looks like information.
  $out = & g++ @flags -c $p[0] -o $p[1] 2>&1
  if (-not (Test-Path $p[1])) {
    Write-Host "COMPILE FAILED: $($p[0])"
    $out | Select-Object -Last 40 | ForEach-Object { Write-Host "    $_" }
    $fail++
  }
}
if ($fail -gt 0) { throw "$fail translation unit(s) failed" }
Write-Host "compiled $($pairs.Count) translation units"

$objs = (Get-ChildItem $bd -Filter '*.o' | ForEach-Object { $_.FullName })
$exe = Join-Path $bd 'matpub.exe'
& g++ -o $exe @objs -lpthread 2>&1 | Select-Object -First 8
if (-not (Test-Path $exe)) { throw 'link failed' }
Write-Host "linked $($objs.Count) objects"

Write-Host '--- run ---'
$env:PATH = "$gxx;$env:PATH"
& $exe
$rc = $LASTEXITCODE
Write-Host "MATPUB_RC=$rc"
exit $rc
