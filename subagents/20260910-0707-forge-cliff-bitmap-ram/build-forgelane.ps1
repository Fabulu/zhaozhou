# build-forgelane.ps1 -- standalone Verilator builds for the FORGE.CLIFF
# bitmap-RAM lane (Roadmap Commit6). Same recipe as RUN-20260910-0631's
# build-culllane.ps1: verilator_bin.exe --build into a gitignored build-*/
# tree, ABSOLUTE include paths (the compile runs from the Mdir), -std=gnu++17,
# no spaces in any path, shared build/ never touched. --assert is ON so the
# candidate's simulation-only invariants are live.
#
# Modes:
#   single : one RTL file verilated under $Prefix with one test .cpp
#            (the prefix trick: verilate the candidate or a mutant as
#            Vzhao_forge_cliff so the UNMODIFIED golden suites drive it).
#   pair   : golden (--cc, static lib) + candidate (--exe --build) in ONE exe,
#            for tests/forge/forge_cliff_ram_differential.cpp.
#
# RUN-TIME DLL TRAP (RUN-20260910-0631): oss-cad-suite\bin first on PATH shadows
# the winlibs libstdc++-6.dll -> exit 0xC0000139 with no message. -Run puts
# winlibs back in front before executing.
param(
  [Parameter(Mandatory)][string]$Name,
  [ValidateSet('single','pair')][string]$Mode = 'single',
  [string]$Rtl    = 'fpga/rtl/forge/zhao_forge_cliff_ram.sv',
  [string]$Top    = 'zhao_forge_cliff_ram',
  [string]$Prefix = 'Vzhao_forge_cliff',
  [string]$Cpp    = 'tests/forge/forge_cliff_directed.cpp',
  [string[]]$RunArgs = @(),
  [string[]]$Extra = @(),      # extra verilator args, e.g. '--x-initial','unique'
  [switch]$Run
)
. C:\programmieren\zencrifice\zhaozhou\tools\env\zhao-env.ps1 | Out-Null
$root = 'C:/programmieren/zencrifice/zhaozhou'
$mdir = "$root/build-forgelane/$Name"
if (Test-Path $mdir) { Remove-Item -Recurse -Force $mdir }   # never a stale object
New-Item -ItemType Directory -Force $mdir | Out-Null
$inc = "-std=gnu++17 -I$root/tests/harness -I$root/reference/include -I$root/reference/src -I$root/runtime/include"
$zref = @("$root/tests/harness/zhao_sim.cpp", "$root/reference/src/zterrain/terrain_core.cpp")
$filter = { $_ -notmatch 'warning|note:|^\s*\d+ \||^\s*\||redefined|In file included|from C:' }

if ($Mode -eq 'pair') {
  # 1. the golden as a static library under its own prefix
  New-Item -ItemType Directory -Force "$mdir/g" | Out-Null
  & verilator_bin.exe --cc --assert @Extra --top-module zhao_forge_cliff --prefix Vzhao_forge_cliff -Mdir "$mdir/g" `
      -CFLAGS $inc "$root/fpga/rtl/forge/zhao_forge_cliff.sv" 2>&1 | Where-Object $filter | Select-Object -Last 2
  & make -C "$mdir/g" -f Vzhao_forge_cliff.mk -j 8 Vzhao_forge_cliff__ALL.a 2>&1 | Where-Object $filter | Select-Object -Last 2
  if (-not (Test-Path "$mdir/g/Vzhao_forge_cliff__ALL.a")) { Write-Host "BUILD_RC=1 (golden lib missing)"; exit 1 }
  # 2. the candidate with the test, linking the golden lib
  New-Item -ItemType Directory -Force "$mdir/c" | Out-Null
  & verilator_bin.exe --cc --exe --build --assert @Extra -j 8 --top-module zhao_forge_cliff_ram --prefix Vzhao_forge_cliff_ram -Mdir "$mdir/c" `
      -CFLAGS "$inc -I$mdir/g" `
      "$root/fpga/rtl/forge/zhao_forge_cliff_ram.sv" "$root/$Cpp" @zref "$mdir/g/Vzhao_forge_cliff__ALL.a" `
      -o "test_$Name.exe" 2>&1 | Where-Object $filter | Select-Object -Last 4
  $brc = $LASTEXITCODE
  $exe = "$mdir/c/test_$Name.exe"
} else {
  & verilator_bin.exe --cc --exe --build --assert -j 8 --top-module $Top --prefix $Prefix -Mdir $mdir `
      -CFLAGS $inc `
      "$root/$Rtl" "$root/$Cpp" @zref `
      -o "test_$Name.exe" 2>&1 | Where-Object $filter | Select-Object -Last 4
  $brc = $LASTEXITCODE
  $exe = "$mdir/test_$Name.exe"
}
Write-Host "BUILD_RC=$brc"
if ($Run -and (Test-Path $exe)) {
  $env:PATH = "C:\programmieren\dsstuff\mingw64\bin;$env:PATH"
  Write-Host "=== RUN $Name ==="
  & $exe @RunArgs
  Write-Host "TEST_RC=$LASTEXITCODE"
}
exit $brc
