# run_cmd_exec_directed.ps1 -- build and run CMD.EXEC's directed test on THIS
# machine, without a CMake tree.
#
# WHY THIS EXISTS, and it is RULING R60's own reason. The static gate set is all
# Python plus Verilator lint, and it stayed green through a merge that broke
# `cmd_exec_directed`'s braces -- so the test was not running at all. R60
# therefore requires this target to BUILD AND RUN at the commit a packet pushes,
# and names it explicitly.
#
# It has been quoted as "not run" by four consecutive packets on entry I34, and
# the reason was never reluctance: `cmake --preset windows-native` in a fresh
# worktree verilates the WHOLE suite, which is hours. This is the short path,
# on `tests/terrain/run_terrain_matpub_directed.ps1`'s pattern, and it exists so
# the next packet does not inherit the same excuse.
#
# THE ONE DIFFERENCE from the matpub runner: this test links `zhao_zref` as well
# as `zhao_harness`, because cases 32-35 differential the viewport lowering
# against `zref::render::viewports_of()` -- the ORACLE `spec/video_rules.md` 3.2
# names -- rather than against a retyped copy of the table. The source list
# below is `reference/CMakeLists.txt`'s, read from it rather than guessed.
#
# The three machine facts are copied from the smoke script rather than
# re-derived: no `make` here so `--cc --exe` and not `--binary`; winlibs g++
# needs the explicit _GLIBCXX_USE_CXX11_ABI; and the unit list to compile is
# `V<top>_classes.mk`, not every .cpp. The PATH order flips between verilate and
# run -- oss-cad-suite's own libstdc++ ahead of winlibs makes the exe die at
# load with 0xC0000139, which reads exactly like a bench crash and is not one.

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $here '..\..')).Path
$R = $Repo.Replace('\', '/')

$vr  = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
$vl  = 'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe'
$gxx = 'C:\programmieren\dsstuff\mingw64\bin'
$env:VERILATOR_ROOT = $vr
$env:PATH = "C:\programmieren\zencrifice\.tools\oss-cad-suite\bin;C:\programmieren\zencrifice\.tools\oss-cad-suite\lib;$gxx;$env:PATH"

$top = 'tb_cmd_exec_pair'
# PER CHECKOUT: two worktrees verilating into one directory fail each other's
# link with an undefined-reference that reads exactly like a partition bug in
# your own change.
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
  "$R/fpga/rtl/command/zhao_cmd_decoder.sv",
  "$R/fpga/rtl/command/zhao_cmd_exec.sv",
  "$R/tests/command/tb_cmd_exec_pair.sv"
)

Write-Host 'verilating'
$ErrorActionPreference = 'Continue'
& $vl --cc --exe --timing --timescale 1ns/1ps -Wno-fatal `
      --Mdir ($bd.Replace('\','/')) --top-module $top --prefix "V$top" `
      "-I$R/fpga/rtl/common" "-I$R/fpga/rtl/generated" `
      @srcs "$R/tests/command/cmd_exec_directed.cpp" 2>&1 |
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
  "-I$Repo/tests/harness", "-I$Repo/tests/command",
  "-I$Repo/reference/include", "-I$Repo/reference/src",
  "-I$Repo/runtime/include",
  '-std=c++17',
  '-DVERILATOR=1','-DVM_COVERAGE=0','-DVM_SC=0','-DVM_TIMING=1','-DVM_TRACE=0',
  '-DVM_TRACE_FST=0','-DVM_TRACE_VCD=0','-DVM_TRACE_SAIF=0','-DVM_VPI=0',
  '-DVL_TIME_CONTEXT','-D_GLIBCXX_USE_CXX11_ABI=0',
  '-faligned-new','-fcf-protection=none','-fcoroutines','-w')

# `reference/CMakeLists.txt`'s own zhao_zref list, in its order.
$zref = @(
  'src/zref_frame.cpp','src/zref.cpp','src/zref_audio.cpp','src/zref_video.cpp',
  'src/zfield/zfield_decode.cpp','src/zfield/zfield_host_plan.cpp',
  'src/zfield/zfield_interpret.cpp','src/zfield/zfield_plan.cpp',
  'src/zterrain/terrain_core.cpp','src/znav/nav_service.cpp',
  'src/zrender/render_frame.cpp','src/zrender/rast.cpp','src/zrender/edgewalk.cpp',
  'src/zrender/geom.cpp','src/zrender/terrain.cpp','src/zrender/sprites.cpp',
  'src/zrender/resolve.cpp','src/zrender/tilestore.cpp','src/zrender/tileresolve.cpp',
  'src/zrender/earlyz.cpp','src/zrender/fragment.cpp','src/zrender/texture.cpp',
  'src/zsky/emit_layers.cpp','src/zsky/star_gamut.cpp','src/zsky/star_bake.cpp',
  'src/zsky/star_flare.cpp','src/zsky/star_field.cpp','src/zsky/star_compose.cpp',
  'src/zsky/env_state.cpp','src/zcreature/creature_core.cpp',
  'src/zcreature/creature_sim.cpp')

Get-ChildItem $bd -Filter '*.o' | ForEach-Object { [IO.File]::Delete($_.FullName) }
$pairs = @()
foreach ($u in $units) { $pairs += , @((Join-Path $bd "$u.cpp"), (Join-Path $bd "$u.o")) }
foreach ($f in @('verilated','verilated_dpi','verilated_threads','verilated_timing')) {
  $pairs += , @("$vr/include/$f.cpp", (Join-Path $bd "$f.o"))
}
$pairs += , @("$Repo/tests/harness/zhao_sim.cpp", (Join-Path $bd 'zhao_sim.o'))
$i = 0
foreach ($z in $zref) {
  $pairs += , @("$Repo/reference/$z", (Join-Path $bd ("zref_$i.o")))
  $i++
}
$pairs += , @("$Repo/tests/command/cmd_exec_directed.cpp", (Join-Path $bd 'cmd_exec_directed.o'))

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
$exe = Join-Path $bd 'cmd_exec.exe'
& g++ -o $exe @objs -lpthread 2>&1 | Select-Object -First 12
if (-not (Test-Path $exe)) { throw 'link failed' }
Write-Host "linked $($objs.Count) objects"

Write-Host '--- run ---'
$env:PATH = "$gxx;$env:PATH"
& $exe
$rc = $LASTEXITCODE
Write-Host "CMD_EXEC_RC=$rc"
exit $rc
