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

#
# ---------------------------------------------------------------------------
# -Mutant: THE POSITIVE CONTROL FOR `terr_pl_slot_overflow_o`
# ---------------------------------------------------------------------------
# With -Mutant the same bench, the same stimulus and the same closure are built
# against `tests/mutants/zhao_console_core_slot_overflow_mutant.sv` -- a WRAPPER
# that instantiates `zhao_console_core` with TERR_POOL_SLOTS halved -- selected
# by a plain `-D` against a plain `ifdef`. Its polarity is INVERTED: the run
# passes when the counter FIRES and fails when it reads 0.
#
# The negative control is this script WITHOUT the switch: the identical
# stimulus against unmutated production must read the counter 0. Both halves
# are required before the zero may be quoted as evidence.
#
# It builds into its own directory so the two sets of Verilator objects cannot
# be mistaken for each other -- a stale object from the other build is the
# stale-binary trap with a mutant's name on it.
#
# ---------------------------------------------------------------------------
# -NoTableLoad: THE NEGATIVE CONTROL FOR EVERY PART.TABLE CHECK
# ---------------------------------------------------------------------------
# Added 2026-09-19 with the composition that closed core header entries I2 and
# I3. The bench now LOADS `zhao_part_table` instead of driving descriptors onto
# the consumers, so its STICK contacts, its collision spawns and its exact
# colour byte are claims about what the table served. A claim like that is worth
# nothing until the check has been seen to FAIL.
#
# With -NoTableLoad the four loads are skipped and NOTHING else changes
# (`+define+ZHAO_SMOKE_SKIP_TBL_LOAD`, a plain `ifdef` -- CLAUDE.md records that
# a command-line define cannot override a FUNCTION-LIKE `define` and says
# nothing when it fails to). Its polarity is INVERTED: the control PASSES when
# the run fails.
#
# MEASURED 2026-09-19: loads[upd/col/spw/crv]=[0 0 0 0], contacts_stick=0,
# spawn_by_event=[0 0 0 0], no children, and the run stops at "a collision
# produced no child". It builds into its own directory so a stale object from
# the loaded build cannot be mistaken for it.
#
# ---------------------------------------------------------------------------
# -BadDescriptor: THE POSITIVE CONTROL FOR THE GEOMETRY ASSET PATH
# ---------------------------------------------------------------------------
# Added 2026-09-19 with the composition that closed core header entry I23. The
# bench now writes a meshlet descriptor and its footprint into a behavioural
# SDRAM and lets GEOM.MESHFETCH, GEOM.MEM_ADAPTER and GEOM.ASSETFETCH read them
# through the real MEM.GUARD -- so ten counters are asserted ZERO, and a
# counter asserted zero is a claim.
#
# With -BadDescriptor ONE byte of the descriptor's reserved span (36..59) is
# poked nonzero IN MEMORY and nothing else changes
# (`+define+ZHAO_SMOKE_BAD_DESC`, a plain `ifdef`). Its polarity is INVERTED:
# the control PASSES when the run fails.
#
# It is worth more than a refusal check, and this is why it is the switch that
# was chosen: if the machine were not really reading the bytes this bench
# poked, changing one of them could not change the outcome. So it fires the
# refusal AND proves the memory path is the descriptor's source.
#
# MEASURED 2026-09-19: refused[fmt/crc/gen/vc/tc/resv/bound]=[0 0 0 0 0 1 0],
# meshlets=0, beats=0, decoded=0, and the run stops at "GEOM.MESHFETCH refused
# the fixture descriptor". The negative control is this script WITHOUT the
# switch: considered=1 fetched=1 culled=0, all seven refusals 0, beats=24,
# decoded=4. Both halves are required before the zeros may be quoted.
# ---------------------------------------------------------------------------
# -BadAttribute: RETIRED 2026-09-19 (geom2 packet, core entry I46 CLOSED)
# ---------------------------------------------------------------------------
# It made the bench's MODEL of the vertex-attribute store answer one lookup a
# clock late, so GEOM.REPLAY's `att_skew_o` could be seen to fire in
# composition. The store is no longer modelled: `zhao_geom_vattr` is composed
# inside the core, so there is no bench-side store left to delay. The detector
# is still fired by stimulus in tests/geometry/geom_replay_directed.cpp (case
# I), and the plain run still asserts it zero. The switch is removed rather
# than kept, because an inverted-polarity control whose define no longer
# changes anything would PASS by failing for an unrelated reason -- or fail
# forever -- and neither is evidence.
# ---------------------------------------------------------------------------
# -BadVertex: THE R31 CONTROL -- one refused vertex record must NOT deadlock
# ---------------------------------------------------------------------------
# Added 2026-09-19 (geom2 packet, owner ruling R31). Record 3 of the fixture
# meshlet gets one nonzero reserved byte IN SDRAM (`+define+ZHAO_SMOKE_BAD_VERTEX`,
# a plain `ifdef`) and nothing else changes. GEOM.VDECODE refuses it; before the
# fix GEOM.GROUP_SEQ then waited for a vertex that could never arrive and the
# whole geometry path stopped -- the run died at "GEOM.REPLAY released no
# meshlet". Now the refusal reaches GROUP_SEQ as a HOLE, both groups are handed
# over POISONED, GEOM.REPLAY drops the batch's triangles and releases it.
#
# Its polarity is DIRECT, unlike the three above: the bench asserts the CORRECT
# behaviour (refused=1, holes=1, groups_poisoned=2, replay_poisoned=SGF_N_TRIS,
# pixels=0, frames_admitted=1) and passes only if all of it holds. The negative
# control is the plain run, which asserts all four R31 counters ZERO.#[CmdletBinding()]
param(
  [string]$Repo    = $null,
  [string]$BuildIn = $null,
  [switch]$SkipVerilate,
  [switch]$Mutant,
  [switch]$NoTableLoad,
  [switch]$BadDescriptor,
  [switch]$BadVertex
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

if (-not $BuildIn) {
  $tag = if ($Mutant) { 'zhao_console_core_smoke_mut' }
         elseif ($NoTableLoad) { 'zhao_console_core_smoke_notbl' }
         elseif ($BadDescriptor) { 'zhao_console_core_smoke_baddesc' }
         elseif ($BadVertex) { 'zhao_console_core_smoke_badvtx' }
         else { 'zhao_console_core_smoke' }
  # PER CHECKOUT. The default used to be one %TEMP% directory for every
  # checkout on the machine, so concurrent packets in separate worktrees
  # verilated into the SAME object directory and failed each other's link with
  # `undefined reference to ...::ctor` -- which reads exactly like a partition
  # bug in your own change (2026-09-19, texmat lane). The repo path's hash
  # keeps one checkout's reruns incremental and two checkouts apart.
  $sha = [System.Security.Cryptography.SHA1]::Create()
  $key = ($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Repo.ToLowerInvariant())) |
          Select-Object -First 4 | ForEach-Object { $_.ToString('x2') }) -join ''
  $BuildIn = Join-Path $env:TEMP "${tag}_$key"
}
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
# THE BEHAVIOURAL SDRAM, and it is added HERE and deliberately not to
# design/fit_targets.yml. sim/models/zhao_sdram_model.sv is testbench-only and
# non-synthesizable, and its own banner forbids it in any synthesis file list;
# the closure above IS the fit's list, so the two must not be the same set. The
# bench needs it because the geometry asset path (core connected item 11) reads
# real memory through the shell's SDRAM controller: without a model behind
# `phy_*` the fetchers elaborate and never see a beat, which is the exact
# failure core entry I23 refused to ship.
$srcs += "$repoFwd/sim/models/zhao_sdram_model.sv"
$srcs += "$repoFwd/tests/prod/tb_zhao_console_core_smoke.sv"
$defs = @()
if ($Mutant) {
  $srcs += "$repoFwd/tests/mutants/zhao_console_core_slot_overflow_mutant.sv"
  $defs += '-DZHAO_MUT_SLOT_OVERFLOW'
  Write-Host 'MUTANT BUILD: zhao_console_core_slot_overflow_mutant, INVERTED POLARITY (passes when the counter fires)'
}
if ($NoTableLoad) {
  $defs += '+define+ZHAO_SMOKE_SKIP_TBL_LOAD'
  Write-Host 'NEGATIVE CONTROL: PART.TABLE is NOT loaded, INVERTED POLARITY (passes when the run FAILS)'
}
if ($BadVertex) {
  $defs += '+define+ZHAO_SMOKE_BAD_VERTEX'
  Write-Host 'R31 CONTROL: ONE vertex record carries a nonzero reserved byte IN SDRAM, DIRECT polarity (passes when the batch drops and the frame completes)'
}
if ($BadDescriptor) {
  $defs += '+define+ZHAO_SMOKE_BAD_DESC'
  Write-Host 'POSITIVE CONTROL: ONE reserved byte of the meshlet descriptor is nonzero IN SDRAM, INVERTED POLARITY (passes when the run FAILS)'
}
Write-Host "closure: $($srcRel.Count) RTL sources from fit_targets.yml"

if (-not $SkipVerilate) {
  # `--cc --exe --main --timing` is deliberately used INSTEAD of `--binary`:
  # --binary would shell out to make, which this box does not have. These
  # flags produce exactly the same sources, main and unit list and stop there.
  Write-Host 'verilating'
  $ErrorActionPreference = 'Continue'
  # -I tests/prod: the bench `include`s smoke_geom_fixture.svh, the generated
  # geometry fixture (tests/prod/smoke_geom_fixture_gen.cpp).
  & $vl --cc --exe --main --timing --timescale 1ns/1ps -Wno-fatal @defs "-I$repoFwd/tests/prod" `
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
if ($NoTableLoad) {
  # INVERTED. A zero here would mean the PART.TABLE checks pass with the table
  # never loaded -- which would make them evidence about something other than
  # what the table served.
  if ($rc -eq 0) {
    Write-Host 'NEGATIVE CONTROL FAILED: the run PASSED with PART.TABLE unloaded. The table checks are not measuring the table.'
    exit 1
  }
  Write-Host "NEGATIVE CONTROL PASS: the run failed (rc=$rc) with PART.TABLE unloaded, as it must."
  exit 0
}
if ($BadDescriptor) {
  # INVERTED. A zero here would mean GEOM.MESHFETCH's reserved-byte refusal row
  # cannot fire -- and, worse, that the descriptor the machine validates is not
  # the one this bench wrote into SDRAM, because changing a byte of it changed
  # nothing. Both halves of the asset path's evidence rest on this.
  if ($rc -eq 0) {
    Write-Host 'POSITIVE CONTROL FAILED: the run PASSED with a nonzero reserved byte in the descriptor. Either the refusal row cannot fire or the machine is not reading the bytes this bench poked.'
    exit 1
  }
  Write-Host "POSITIVE CONTROL PASS: the run failed (rc=$rc) with one corrupted descriptor byte, as it must."
  exit 0
}
exit $rc
