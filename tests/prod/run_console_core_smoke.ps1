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
# CHANGED 2026-09-19 evening (owner ruling R42, core entry I33). The bench no
# longer LOADS the table at all -- the descriptors are a SPECIES_TABLE page it
# stages and PUBLISHES, and zhao_part_table_loader reads them back. So the
# control is no longer "skip the loads"; it is a page the loader must REFUSE.
# With -NoTableLoad one byte of the page's magic is wrong and NOTHING else
# changes (`+define+ZHAO_SMOKE_BAD_SPECIES_PAGE`, a plain `ifdef` -- CLAUDE.md records that
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
# -NoEchoArm: THE NEGATIVE CONTROL FOR POST.ECHO'S ARM (owner ruling R35)
# ---------------------------------------------------------------------------
# Added 2026-09-19 with SetPost (R36). The bench's command packet carries a
# SetPost whose flags ARM the echo, and the armed run compares the capture
# against the frame word for word. That check is evidence about the ARM only if
# the capture does not happen without it.
#
# With -NoEchoArm the SAME packet carries flags = 0 and nothing else changes
# (+define+ZHAO_SMOKE_NO_ECHO_ARM, a plain ifdef). Its polarity is NORMAL:
# the bench asserts that POST.ECHO opened no pass and wrote no pixel, and the
# run PASSES. It also prints the post lease's busy clocks with the echo off,
# which is the unarmed half of ruling R38's memory budget.
#
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
  # ---------------------------------------------------------------------------
  # -UntexMutant: THE POSITIVE CONTROL FOR `geom_untex_refused_o` (R197)
  # ---------------------------------------------------------------------------
  # Added 2026-09-20 with the untextured attribute law. The refusal counter at
  # GEOM.CLIP's door cannot move under legal stimulus while the only composed
  # producer (GEOM.REPLAY, format 0) declares TEXTURED, so this form builds the
  # same bench and closure against
  # `tests/mutants/zhao_console_core_untex_decl_mutant.sv` -- a WRAPPER with
  # GEOM_REPLAY_UNTEX_DECL = 1 -- selected by `-DZHAO_MUT_UNTEX_DECL`. Its
  # polarity is INVERTED: it passes when the counter reaches the reference's
  # replayed count and NOTHING enters GEOM.CLIP; the plain run is the negative
  # control and asserts the counter ZERO. Own build directory, own TAG.
  [switch]$UntexMutant,
  [switch]$NoTableLoad,
  [switch]$BadDescriptor,
  [switch]$BadVertex,
  [switch]$NoEchoArm,
  [switch]$BadTraceArm,
  # ---------------------------------------------------------------------------
  # -GlowTag: THE FIRST LIT FRAGMENT THE COMPOSED CONSOLE HAS EVER CARRIED
  # ---------------------------------------------------------------------------
  # Added 2026-09-21 (tagprod), and it is the form POSTGATHER costed and could
  # not build. That packet composed POST.GATHER and its smoke read
  # `frags=2560 [untagged=2560 below_knee=0 lit=0 reserved=0]` -- CORRECT, and
  # for a reason outside POST.GATHER: the effect tag is
  # `tri_continuation_tail_i[15:8]`, core entry I20's open boundary, and this
  # bench drives it `'0`. So owner ruling R195's law was verified EXHAUSTIVELY
  # at block level (2^24 tag/colour pairs) and the SEAM was verified in the
  # console, and NOTHING IN THIS TREE DID BOTH AT ONCE.
  #
  # With -GlowTag the bench sets TWO FIELDS of that one port and nothing else:
  # `effect_tag` = 0x7F (the frozen `tag = (channel << 6) | strength` of
  # spec/stars_and_flares.md 1: GLOW = 0b01, strength 63, so R195's ramp gives
  # gain 68) and `vertex_rgb` = 0xB5AAB5. The colour is necessary, not
  # decorative: the glow BORROWS the fragment's own colour, and with the tail at
  # zero the fragments are BLACK, so the tag alone would move a counter and
  # light nothing. The bench's header explains why that exact value -- it
  # survives the ordered dither on every Bayer phase, it can never collide with
  # the odd frame sentinel, and it does not saturate the bloom away.
  #
  # Its polarity is DIRECT, and it INVERTS ONE ASSERTION rather than removing
  # it: in every other form the post pass is an IDENTITY and `fb_bad` must be 0;
  # here the plane is lit and the pass must CHANGE pixels. The plain run is the
  # negative control and asserts `untagged == fragments`,
  # `bloom_cells_contributing == 0` and that NO pixel carries the tail colour.
  #
  # MEASURED 2026-09-21. -GlowTag: `frags=2560 [untagged=1498 below_knee=0
  # lit=1062 reserved=0]`, 1,062 pixels at 0xB556, 1,344 bloom cells, 1,344
  # framebuffer words changed. Plain: `[untagged=2560 ... lit=0]`, 0 pixels at
  # 0xB556, 0 bloom cells, 0 words changed. BOTH HALVES ARE REQUIRED before
  # either number may be quoted.
  #
  # AND NOT EVERY RESOLVED FRAGMENT IS LIT, WHICH IS CORRECT. RASTER.RESOLVE
  # sweeps a touched TILE WHOLE, so `gather_fragments_o` counts all 256 pixels
  # of each of the ten tiles this fixture enters; only 1,062 are covered and the
  # rest carry the tile clear, tag 0. The first version of the bench assertion
  # demanded all 2,560 and failed -- the assertion was wrong, not the console,
  # and the bench now says so where the next reader will find it.
  #
  # It also settles a question no identity pass could answer -- whether
  # POST.ECHO taps the compositor's SOURCE or its OUTPUT. See the bench.
  # ---------------------------------------------------------------------------
  # -TerrainFlatLattice: THE ZERO BODY, KEPT AS A NAMED CONTROL
  # ---------------------------------------------------------------------------
  # REPLACES -TerrainRelief, 2026-09-26 (TERRAINVISIBLE), and the check is the
  # same one -- it is the FIXTURE that inverted, not the standard.
  #
  # PROJCOLLAPSE added `-TerrainRelief` to prove, by reversal, that the played
  # pages' all-zero layer A was what made every terrain triangle zero-area: the
  # eye sits at world y = 0, the lattice sat at world y = 0, and a plane through
  # the eye projects to a line. It measured `culled` 256 -> 0.
  #
  # `reports/DECISION-20260926-TERRAIN-FIXTURE.md` then made the relief the
  # DEFAULT, so the plain run is the one with a ground plane and the flat page
  # is the switch. With -TerrainFlatLattice layer A is left all zeros and
  # NOTHING else changes (`+define+ZHAO_SMOKE_TERRAIN_FLAT`, a plain `ifdef` --
  # CLAUDE.md records that a command-line define cannot override a
  # FUNCTION-LIKE `define` and says nothing when it fails to).
  #
  # DIRECT polarity: the run PASSES when GEOM.CLIP culls every terrain triangle
  # for ZERO AREA and takes only the mesh's 14 into GEOM.SETUP. What it asserts
  # is a law of the PROJECTION, which stays true forever -- it is not a test
  # that asserts a bug, because the flatness is the control's own stimulus, the
  # way -BadVertex pokes a reserved byte.
  #
  # It is also the positive control for a counter the plain run now asserts
  # ZERO: `geom_clip_culled_o`. Both halves are required before that zero may
  # be quoted.
  #
  # NOTE: `raster pixels` in this form is the MESH's 2560, not the plain run's
  # 2816, and must not be read as the gated number. Own build directory, own
  # TAG (see the paragraph in the tag chain).
  [switch]$TerrainFlatLattice,
  [switch]$GlowTag,
  # ---------------------------------------------------------------------------
  # -LintOnly: THE CHEAP HALF, AND IT BELONGS FIRST (owner ruling R71)
  # ---------------------------------------------------------------------------
  # Added 2026-09-20. Three merges in one run swallowed a closing construct --
  # a brace in `cmd_exec_directed`, `spt_entry`'s `end`/`endfunction`, and a
  # whole superseded `tbl_load` task dragged back in from an older branch --
  # and the ENTIRE static gate set stayed green through all three, because it
  # is Python plus Verilator lint over `fpga/rtl` and none of it elaborates a
  # bench. Each was found by a ten-minute run or a full build.
  #
  # This stops after the verilate step and reports its exit code. MEASURED
  # 2026-09-20: 23 s clean against the full run's ~10 min, and 1 s when it
  # fails. So it goes FIRST on the merge checklist: a cheap gate that runs
  # always beats an expensive one that runs eventually. It is NOT a substitute
  # for the run -- it proves the tree ELABORATES, and says nothing whatever
  # about what the console then does.
  #
  # A PASS IN A FRACTION OF A SECOND IS REAL, AND IT IS NOT NOTHING.
  # Verilator's `--skip-identical` is CONTENT-hashed, so a merge that touches
  # no file in the closure makes this return in about 0.3 s. That reads
  # exactly like a gate that has stopped doing anything, so it was proved
  # rather than assumed: planting `this_is_not_systemverilog endmodule` in
  # `zhao_console_core.sv` makes the SAME warm directory return nonzero with a
  # real `%Error` in 0.7 s. The cache hit is evidence from the elaboration it
  # matched, not an absence of one. Touching the mtime does NOT invalidate it;
  # only the content does.
  #
  # SEEN TO FIRE, not assumed. `spt_entry`'s `end`/`endfunction` was deleted
  # from the bench on purpose -- the exact damage the 2026-09-20 hostdbg merge
  # did -- and this switch returned nonzero with
  # `tb_zhao_console_core_smoke.sv:2416: syntax error, unexpected localparam`
  # in one second. The file was restored in the same invocation; the control is
  # reproducible from this paragraph and leaves no copy to rot.
  [switch]$LintOnly,

  # UNRECOGNISED ARGUMENTS ARE A HARD ERROR, and this catch-all is the whole
  # reason the check below can exist. Added 2026-09-21 after FIELDLANE found
  # `console_core_attrpack_control` -- a registered, labelled, GREEN ctest --
  # measuring NOTHING since 2026-09-19. It passes `-BadAttribute`, which line
  # 121 of this very file records as RETIRED that day.
  #
  # THE MECHANISM, measured rather than assumed:
  #
  #   powershell -NoProfile -File script.ps1 -TotallyUndeclared   ->  RC 0
  #   powershell -NoProfile -File script.ps1 -Bogus 5             ->  RC 0
  #
  # `-File` BINDS NOTHING AND SAYS NOTHING. No error, no warning, no non-zero
  # exit -- the script simply runs as though the flag had not been typed. So a
  # retired flag, a typo, or a form that never existed all produce a PASSING
  # PLAIN RUN that is indistinguishable, in a gate list, from a passing
  # TARGETED run. That is the broken-instrument law with the toolchain holding
  # the knife: the failure is silent and in the flattering direction.
  #
  # It is quieter than the trap it replaced. Passing a flag through a shell
  # VARIABLE to `& .\script.ps1` at least binds positionally as $Repo and
  # usually breaks loudly; an undeclared switch under `-File` is pure silence.
  #
  # Proven in both directions before being committed here: a declared switch,
  # a positional $Repo and a named $Repo all still bind (RC 0), and
  # `-BadAttribute` now exits 2 with the name printed.
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$Unrecognised
)

if ($Unrecognised) {
  Write-Error ("UNRECOGNISED ARGUMENT(S): " + ($Unrecognised -join ', ') +
               "`nThis script accepts only the switches in its param() block." +
               "`nA retired or mistyped form used to run the PLAIN smoke and" +
               " PASS. It no longer does. See the note above this check.")
  exit 2
}

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
         elseif ($UntexMutant) { 'zhao_console_core_smoke_untex' }
         elseif ($NoTableLoad) { 'zhao_console_core_smoke_notbl' }
         elseif ($BadDescriptor) { 'zhao_console_core_smoke_baddesc' }
         elseif ($BadVertex) { 'zhao_console_core_smoke_badvtx' }
         elseif ($NoEchoArm) { 'zhao_console_core_smoke_noecho' }
         # WITHOUT THIS ARM, `-BadTraceArm` FELL THROUGH TO THE PLAIN TAG and
         # built into the plain run's object directory -- the exact collision
         # the comment below describes, reintroduced by adding a switch and
         # forgetting its tag. It is silent: the script deletes `*.o` before
         # compiling, so the variants merely rebuild each other rather than
         # failing, and the only symptom is that the two can never run
         # concurrently and that a `smoke.exe` left running by one blocks the
         # other's link. Found 2026-09-20 by reading the running process's
         # PATH, not by a failure. EVERY NEW SWITCH NEEDS A TAG HERE.
         elseif ($BadTraceArm) { 'zhao_console_core_smoke_badarm' }
         # EVERY NEW SWITCH NEEDS A TAG HERE -- see the paragraph above, which
         # is about exactly this line being forgotten once already.
         elseif ($TerrainFlatLattice) { 'zhao_console_core_smoke_flatlat' }
         elseif ($GlowTag) { 'zhao_console_core_smoke_glow' }
         else { 'zhao_console_core_smoke' }
  # -LintOnly is the one switch that COMBINES with the others, so it appends
  # rather than joining the chain above. Without this it would fall through to
  # whichever tag its companion chose and verilate into a directory a real run
  # may be compiling in -- the same collision, one switch later.
  if ($LintOnly) { $tag = "${tag}_lint" }
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
if ($UntexMutant) {
  $srcs += "$repoFwd/tests/mutants/zhao_console_core_untex_decl_mutant.sv"
  $defs += '-DZHAO_MUT_UNTEX_DECL'
  Write-Host 'MUTANT BUILD: zhao_console_core_untex_decl_mutant, INVERTED POLARITY (passes when geom_untex_refused_o reaches the replayed count and nothing enters GEOM.CLIP)'
}
if ($NoTableLoad) {
  $defs += '+define+ZHAO_SMOKE_BAD_SPECIES_PAGE'
  Write-Host 'NEGATIVE CONTROL: the SPECIES_TABLE page has a wrong magic byte, so PART.TABLE is never loaded. INVERTED POLARITY (passes when the run FAILS)'
}
if ($BadVertex) {
  $defs += '+define+ZHAO_SMOKE_BAD_VERTEX'
  Write-Host 'R31 CONTROL: ONE vertex record carries a nonzero reserved byte IN SDRAM, DIRECT polarity (passes when the batch drops and the frame completes)'
}
if ($NoEchoArm) {
  $defs += '+define+ZHAO_SMOKE_NO_ECHO_ARM'
  Write-Host 'NEGATIVE CONTROL: the SetPost leaves POST.ECHO DISARMED (R35); the bench asserts no capture happens'
}
if ($GlowTag) {
  $defs += '+define+ZHAO_SMOKE_GLOW_TAG'
  Write-Host 'R195 END-TO-END FROM THE ABI: the uploaded MaterialRecord declares a fragment profile whose effect_tag is a GLOW tag (0x7F), DIRECT polarity (passes when gather_frag_lit_o EQUALS the framebuffer pixels carrying a colour, the bloom stage finds cells, and the post pass CHANGES the frame). The declared state word is the all-zero opaque profile, so the tag is the ONLY variable that moves between this form and the plain one -- and because that word is bit-identical to declaring nothing, this form is also the positive control for reading fragment_decl bit 0 rather than testing the payload for zero.'
}
if ($TerrainFlatLattice) {
  $defs += '+define+ZHAO_SMOKE_TERRAIN_FLAT'
  Write-Host 'TERRAINVISIBLE CONTROL (entry I13): layer A of every played terrain page is left ALL ZEROS -- the body every page carried until 2026-09-26 -- instead of the affine ground ramp the repaired fixture writes. DIRECT polarity (passes when GEOM.CLIP culls EVERY terrain triangle for ZERO AREA and GEOM.SETUP takes only the mesh reference): a ground plane through the eye projects to a line. It is the positive control for the zero that the plain run asserts on clip culled. NOTE: raster pixels is the MESH total 2560 in this form, NOT the plain run 2816, and must not be read as the gated number.'
}
if ($BadTraceArm) {
  $defs += '+define+ZHAO_SMOKE_BAD_TRACE_ARM'
  Write-Host 'R52 CONTROL: the DebugTraceArm record sets an UNASSIGNED stage_mask bit, DIRECT polarity (passes when the record is refused whole and nothing is armed)'
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

if ($LintOnly) {
  # Deliberately AFTER the verilate step rather than instead of it: `--cc`
  # elaborates the whole closure, which is what catches an unterminated
  # function. `--lint-only` would be faster still and would NOT have caught
  # the merge damage this switch exists for, because the damage was in a
  # bench that `--lint-only` over `fpga/rtl` never reads.
  Write-Host "LINT-ONLY: $top elaborates. This says NOTHING about what the console does."
  exit 0
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
  # THE COMPILER'S OUTPUT IS KEPT, and it used to be piped to Out-Null.
  # CLAUDE.md, Build note: "Never send a build's output to `Out-Null` -- a
  # build wrapped in a helper that discards its output cannot be seen to have
  # failed or to have done nothing." This script did exactly that, and it cost
  # two undiagnosable runs on 2026-09-20: `COMPILE FAILED: <path>` and nothing
  # else, twice, with the reason already thrown away. A failure report that
  # names the file and withholds the error is worse than a crash, because it
  # looks like information.
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
    Write-Host 'NEGATIVE CONTROL FAILED: the run PASSED with the species page refused and PART.TABLE unloaded. The table checks are not measuring the table.'
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
