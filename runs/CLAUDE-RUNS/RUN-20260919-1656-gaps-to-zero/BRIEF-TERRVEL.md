# TERRVEL — the height-velocity lattice reaches a consumer

**Branch `gz/terrvel`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

## The one number

`python tools/budget/completion_register.py`, run **BARE** (piping reports the
pipe's status; RC 1 while gaps remain is NORMAL). It says **8**. Two of those
eight are `BUILT BUT NOT CONNECTED`, and **`zhao_terrain_velocity` is one of
them.** Your target is that line leaving the list for a reason that is a moving
number and not a wire.

You also own the narrowed live-patch hole inside entry **I34** (below). I34
itself will very probably NOT close — say so plainly if it does not, with what
remains. **A refusal with evidence outranks a number moved by a tie-off.**

## What is already established — and what you must RE-ASK

Read entry **I34** in `fpga/rtl/prod/zhao_console_core.sv` (around line 4422)
in full before anything else. It is long because five packets narrowed it. The
parts that bear on you:

* **Velocity's destination is RATIFIED**: `spec/memory_rules.md` 5b,
  `TERRAIN.COMPOSED_VELOCITY` at `0x056F_0000`, 256 x 2,304 B. The entry says
  *"THE VRAM WRITER BETWEEN THEM DOES NOT EXIST. That is a build, and it is the
  smallest of the three."*
* **AND COMPOSEPUB REFUSED THAT BUILD WITH A BANDWIDTH PROOF**, the same day:
  composed velocity *"has no consumer at any level"*, and charging the write
  (`tools/budget/sdram_bandwidth.py --with-composed-publish`) takes the frame
  from **80.17% to 124.41%** at the flattering hit spans and to **177.49%** once
  the fill-side read that would make it a consumer is counted. Decision record
  in `spec/memory_rules.md` 5b. **A window is opened WITH its block, never ahead
  of it.**
* So **the obvious build is the one that is already refused**, and building it
  anyway would spend a ceiling we do not have on a region nothing reads. Do not
  re-litigate that proof by assertion; if you think it is wrong, re-run that
  tool and show the numbers.

**The route the refusal leaves open is FABRIC, not SDRAM.** COMPOSEPUB's own
words: *"the composed HEIGHT already reaches its consumers THROUGH FABRIC, so an
SDRAM writer would be read by nothing."* Height's fabric consumer is
`zhao_terrain_heighttap` — TERRAIN.TAPSHARE's single service, whose clients are
**PART.COLLIDE and FORGE.SHADOW**. And `spec/terrain_rules.md` §4.4 says velocity
is *"interpolated by the same §4.3 rule"*, and that **the interpolation is the
CONSUMER's (`column_query`)** — which is exactly what the tap implements.

I checked one thing myself so you do not have to: **`zhao_terrain_heighttap` has
no velocity response port today.** Its `rsp_*` set is height, no-ground, the
normals, the four corner heights and the debug lattice. That is a seam, not a
conclusion.

**RE-ASK EVERY ABSENCE CLAIM ABOVE BEFORE BUILDING ON IT.** In this tree the
"X does not exist" claims run false at a rate near one in two, and the pattern is
always *true when written, nobody re-asked*. TERRAINAUX, the packet that just
landed, found *"the aux context has no producer anywhere"* to be **HALF FALSE** —
the consumer chain was composed and resident the whole time and the missing wire
was in two places, both inside `zhao_console_core`. Three refusals sat between
the stamp and the pixel and **each was correct given the other two.** Open the
file that would own the capability and read its ports; a zero-result grep for a
PHRASE is not a search for the CONCEPT.

## The architectural blocker, and your authority over it

I34 records velocity's blocker as architectural rather than a wire:
`zhao_terrain_velocity` *"drives its OWN 33x33 sweep, so joining it to the
consumer's vertex stream is a scheduler and a composer may not write one"*,
routed through directive 13.2's `zhao_terrain_patch_v2`.

**"A composer may not write one" bars the logic from `zhao_console_core`. It does
not bar the logic.** EDGECLOSE hit exactly this shape last week: the island pitch
*could not* be composed because `island_dir` takes the descriptor as an input, so
it **built a new block that seals and checks** instead, and closed two entries.
That precedent is yours. If a scheduler is what joins the sweep to the stream,
**it is a named block with a contract, a test and a ledger entry** — not fifty
lines in the composer.

You are working under `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`, which
gives standing authority over technical decisions **including amending specs and
rulings, provided what is superseded is STATED**. Use TERRAINAUX's pattern: a
decision record naming what it supersedes **and what it does not**.

**What that authority explicitly does NOT cover** — quoting the directive:
*"This is NOT authority to delete a feature, reduce 16 fields to 4, remove
Gouraud/detail normals, shrink the guaranteed giant, replace a live path with
testbench stimulus, waive a correctness failure, or call reduced work equivalent
merely to reach zero or fit a device."* **A velocity lane that is computed and
read by nothing is not connected**, whatever the register says.

**And one decision on this entry is fenced off from you.** I34's item (B) puts
material and nav_cost — owner named, destination unratified, two incompatible
encodings in one tree — to the OWNER as a decision **"a packet may not decide"**.
**Velocity is not in that fence; it is the one of the four with a ratified
destination.** Stay on velocity. If your work forces the material/nav question,
**stop and write it up** rather than answering it.

## The second item: I34's intake-versus-LIVE-PATCH hole

EARTHLOCK repaired intake-versus-REPLAY inside `zhao_field_earth_adapter` and
narrowed what is left to something **this file owns**: if CMD.EXEC begins frame
F+1's TerrainField records while frame F's patch is still composing, F+1's clear
drops F's binding mid-walk. It is LOUD (`noprog_o`) and the repair did not worsen
it. The entry names the two shapes of close: hold the record intake off while a
patch is live, or gate `tce_job_take` on `fld_earth_idle_o`.

**`fld_earth_idle_o` is exported for exactly this and the entry says it has ZERO
READERS IN THE TREE** — core port, board port, nothing consumes it. Verify that
(see the re-ask rule above; it is an absence claim like any other). A dangling
output that exists *for* the close you are making is either your answer or a
finding about why it is not.

The entry's own instruction to whoever takes it: **say what BOUNDS CMD.EXEC's
record stream against the patch schedule rather than assert it is bounded.**

## Terrain's fixture was broken and is now repaired — this is new as of today

Until this morning every terrain bench you might reach for was measuring
degenerate geometry. TERRAINAUX found the cause: **`zhao_terrain_seq` walks a set
ONCE and skips non-resident patches, so one `SubmitTerrainSet` never opens the
compose door, so `compcache_front` answers POISON `32'h5BADF00D` and all 81
vertices are THE SAME POINT.** The fixture now reads `degenerate=128 -> 0`,
`refs_taken 128 -> 256`, `raster pixels=2560` unchanged. The bench's own note
blaming *"zero heights, cross product exactly zero"* was FALSE — a flat lattice
with distinct x/z has an up-facing normal.

**You are the first velocity packet with a working terrain fixture.** Use it.

## Evidence bar

* **A live chain of REAL PRODUCTION MODULES wired port-for-port as
  `zhao_console_core` wires them**, driven to a number that MOVES when the field
  moves. `tests/terrain/composepub_acceptance.cpp` is the pattern to copy: four
  real blocks, 1,089 engine runs, and `live_top - compose_top` equal to the
  field's out-lane 0 at every vertex. `tests/prod/terrainaux_acceptance.cpp` is
  the other pattern: 758 checks ending in a PIXEL that changes.
* **Cross-check against the oracle**, not against yourself:
  `reference/include/zref/zref_terrain_velocity.hpp` and
  `reference/src/zrender/terrain.cpp`'s `field_velocity_lane`. Note the oracle
  builds **no** material and **no** nav lattice — it cannot settle those, which
  is part of why they are fenced.
* **Prove every counter you quote.** A detector reading zero is a claim, and it
  is the claim to check hardest. **Check what CLOCKS the two sides of any
  comparison** — a checker whose operands move together cannot fire. If a guard
  is unreachable with legal stimulus, it needs a **committed mutant** under
  `tests/mutants/`, renamed so no source list elaborates it, with the driver's
  polarity inverted so it passes when the counter FIRES.
  **Register your positive controls with the right polarity** — TERRAINAUX
  registered two backwards (`exit 0` on a caught corruption under
  `WILL_FAIL TRUE`) and ctest caught it in twenty seconds; the other polarity
  would have been a control that cannot fail.
* **Do not write a test that asserts the bug.** Assert the correct behaviour and
  keep the positive control separate.

## Traps that have cost this campaign real time

* **THE GATES DO NOT BUILD.** `python tools/maintenance/gate_sweep.py` returning
  0 means *nothing moved against the baseline*, not *the tree is green*. A green
  sweep sat on a tree that could not configure **twice in two days**. Run
  `cmake --preset windows-native` yourself, from PowerShell with
  `tools/env/zhao-env.ps1` sourced. **Read the build's exit code, not the
  pipeline's** — `cmake --build ... | tail` reports `tail`'s status, and a build
  that died on step 18 of 554 printed `BUILD_RC=0` and was believed.
* **`mutant_copy_drift` keys on COMMIT TIME.** Editing a file even in a comment
  stales its committed copies, and reverting does not clear it. TERRAINAUX
  declined to touch `zhao_texture_material_combine_v3` for exactly this —
  **thirteen** copies. Check the copy count before you edit a hot file, and route
  around it if the edit is cosmetic.
* **Regenerate `zhao_prod_top.sv` after ANY port change**
  (`tools/quartus/gen_prod_top.py`), and re-run
  `tools/quartus/check_prod_manifest.py`. Registering a block in the ledger, the
  manifest and the production source list are **three different acts**.
* **Verilator lint-clean is not Quartus-synthesizable.** Quartus 17.0 needs a
  module-scope elaboration check inside `initial begin ... end` and needs the
  explicit `generate`/`endgenerate` keywords. Both forms passed lint with 0
  diagnostics and failed `quartus_map` in 33 seconds.
* **DO NOT START A QUARTUS FIT.** Fits are the scarce resource and are batched at
  subsystem boundaries by me, not by a packet. Every question you have —
  correctness, throughput in clocks, handshake behaviour, atomicity under
  backpressure — is Verilator and answers in seconds.
* **One `ctest` at a time per build tree**, and if you kill one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*` before restarting. A ctest with no
  child process and frozen CPU is that debris, not a slow suite.
* **Your prose can change the register.** `completion_register.py` classifies
  entries by scanning their text. TERRAINAUX wrote "TIED TO ZERO" about a tie it
  had just REMOVED and relabelled I13 from `boundary` to `tied-to-zero`. **The
  tool was right and the prose was the bug.** Re-run the register bare after
  editing any entry text.

## Deliverable

Commit as you go — the work is the commit messages, since you cannot write report
files. Your final commit message is your FINDINGS and I transcribe it. It must
state, in your own words and without softening:

1. **The register before and after, measured bare**, and whether
   `zhao_terrain_velocity` left the `BUILT BUT NOT CONNECTED` list.
2. **What moved.** The number, the stimulus that moved it, the chain of real
   modules it moved through, and the oracle it agrees with.
3. **Every absence claim in this brief that you found to be FALSE**, and what was
   actually there. This is the most valuable thing you can return.
4. **What you refused and why**, including anything that would have needed the
   fenced material/nav decision or the refused SDRAM publish.
5. **Anything you got wrong and caught yourself.** Both of TERRAINAUX's
   self-caught errors are in its merge commit and they are the reason its report
   is trustworthy.
6. The branch and commit hash. **Push `gz/terrvel` only.** Never rebase or push
   the shared branch, and never `--force`.
