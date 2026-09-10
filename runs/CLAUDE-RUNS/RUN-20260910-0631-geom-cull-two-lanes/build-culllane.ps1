# build-culllane.ps1 -- standalone Verilator build (+ optional run) for the
# geom_cull lane. Same recipe as RUN-20260909-2343's build-poselane/:
# verilator_bin.exe --build into a gitignored build-*/ tree, absolute include
# paths (the compile runs from the Mdir), -std=gnu++17, no spaces in any path.
# Shared build/ is never touched.
#
# RUN-TIME DLL TRAP (found 2026-09-10): zhao-env.ps1 puts oss-cad-suite\bin
# first on PATH, and its older libstdc++-6.dll shadows the winlibs one the exe
# was linked against -> exit 0xC0000139 (ENTRYPOINT_NOT_FOUND) with no message.
# -Run puts winlibs back in front before executing.
param(
  [Parameter(Mandatory)][string]$Name,
  [string]$Rtl    = 'fpga/rtl/geometry/zhao_geom_cull.sv',
  [string]$Top    = 'zhao_geom_cull',
  [string]$Prefix = 'Vzhao_geom_cull',
  [string]$Cpp    = 'tests/differential/geom_cull_directed.cpp',
  [string[]]$Extra = @(),
  [string]$Defs = "",
  [switch]$Run
)
. C:\programmieren\zencrifice\zhaozhou\tools\env\zhao-env.ps1 | Out-Null
$root = 'C:/programmieren/zencrifice/zhaozhou'
$mdir = "$root/build-culllane/$Name"
if (Test-Path $mdir) { Remove-Item -Recurse -Force $mdir }   # never a stale object
New-Item -ItemType Directory -Force $mdir | Out-Null
$cflags = "-std=gnu++17 -I$root/tests/harness -I$root/reference/include -I$root/reference/src -I$root/runtime/include $Defs"
& verilator_bin.exe --cc --exe --build -j 8 --top-module $Top --prefix $Prefix -Mdir $mdir `
    -CFLAGS $cflags @Extra `
    "$root/$Rtl" "$root/$Cpp" "$root/tests/harness/zhao_sim.cpp" "$root/reference/src/zrender/rast.cpp" "$root/reference/src/zrender/texture.cpp" `
    -o "test_$Name.exe" 2>&1 | Where-Object { $_ -notmatch 'warning|note:|^\s*\d+ \||^\s*\||redefined|In file included|from C:' } | Select-Object -Last 4
$brc = $LASTEXITCODE
Write-Host "BUILD_RC=$brc"
if ($Run -and (Test-Path "$mdir/test_$Name.exe")) {
  $env:PATH = "C:\programmieren\dsstuff\mingw64\bin;$env:PATH"
  Write-Host "=== RUN $Name ==="
  & "$mdir/test_$Name.exe"
  Write-Host "TEST_RC=$LASTEXITCODE"
}
exit $brc
