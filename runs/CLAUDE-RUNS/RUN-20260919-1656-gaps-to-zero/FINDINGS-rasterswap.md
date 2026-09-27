# FINDINGS — RASTERSWAP, 2026-09-27

Branch `gz/rasterswap`, worktree `C:\programmieren\zencrifice\gz-rasterswap`,
base `0cc81ee5`. Reported against the brief's nine numbered Deliverable items.

---

## 1. THE REGISTER, MEASURED BARE, BEFORE AND AFTER

```
BEFORE (0cc81ee5)   MANDATORY GAPS REMAINING : 2
                      (2 tie-offs + 0 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable)
                      I34  boundary       TERRAIN.PATCH's FIELD-HEIGHT LANE
                      I55  unclassified   GEOM.PARAMBUF's WALK REQUEST and DECODED OUTPUT

AFTER               MANDATORY GAPS REMAINING : 2   (unchanged)
```

Run bare, never through a pipe -- RC 1 is normal while gaps remain and a pipe
would have reported the LAST stage's status.

**I55 DID NOT CLOSE, and this packet does not claim it did.** Section 7 says
what was refused and why, with numbers from this tree rather than quoted.

---

## 2. THE TIDQ IDS -- REPAIRED AND PROVEN

**Step zero, and not optional: the arena descriptor ids a raster swap would
read were wrong in the composed console, and every gate was green.**

### Reproduced first, at the base commit, before anything was changed

```
SMOKE: tidq       underflow=1 overflow=0 unnamed=0
SMOKE: tidqseam   pushes=75 pops=75 flushes=1 vid_seal_abort=0 | at first underflow: pushes=0 pops=1
SMOKE: tidqids    pushed=0 1 2 3 4 5 6 7 | popped=262143 0 1 2 3 4 5 6
SMOKE: arenabin   tris=74 unnamed=1 refs=97
SMOKE: arenabin   SHORTFALL 4 reference(s) (97 against GEOM.BINNER's 101)
SMOKE: raster     pixels=2816     frames_admitted=1
```

262143 is ID_POISON. After it, popped[k] == pushed[k-1] for every k:
**triangle 1 dropped for want of an identity and 74 of 75 binned under their
PREDECESSOR's arena descriptor index.** Entry I54's named failure, live, in an
entry the register counts as CLOSED -- invisible because the ids stay IN RANGE
and DECODE CLEANLY, so td_illegal_o never fires and the bench's own refs
comparison differences two totals.

### The cause, confirmed as ARENAINFER measured it

Structural STARTUP skew, not a frame edge. The fork hands one triangle to
GEOM.SETUP, GEOM.ATTRPACK and GEOM.VERTID on one clock. GEOM.SETUP is three
stages; GEOM.VERTID needs S_PUB x3 plus S_TD before the arena returns an index.
**The door is always ahead of the id.** vid_seal_abort=0 and pushes == pops --
nothing was lost, the association was simply off by one.

### The repair -- a HANDSHAKE, not a tuned delay

Correct for any skew rather than for the measured one. Three blocks, as costed:

1. **zhao_console_core** -- `tidq_have_w = (tidq_level_w != 0)` gates the join.
   `level_o` was connected to `()`; that is the whole of the defect.
   **All three consumers are gated**: the join's valid AND GEOM.SETUP's
   `out_ready_i` AND GEOM.ATTRPACK's. Gating only the ready lets the shell and
   GEOM.ARENABIN take a triangle the door never released; gating only the valid
   lets SETUP/ATTRPACK retire one the door never took. Both halves DROP a
   triangle rather than misname one, which is worse.
2. **The seal-abort deadlock, closed with ZERO new leaf ports.** `busy_o` was
   already a port of zhao_geom_vertid, connected to `()`. It is
   `(st_q != S_IDLE)`, so `pa_seal_fire && vid_busy_w` IS the abort pulse; it
   pushes a NAMELESS entry so the queue stays 1:1 with the triangle stream.
3. **zhao_geom_tidq** -- the flush **poisons in place instead of discarding**,
   and is no longer an `else if`. Every entry is owed to a triangle already
   inside GEOM.SETUP, so discarding one turns a wrong id into a permanent stall
   the moment the door is gated. `stale_q` withdraws the NAMES and keeps the
   COUNT; a push on the seal clock -- exactly when the abort entry arrives --
   now survives.

**No deadlock, and the argument names what each side waits on.** The push is
gated only by `zhao_geom_paramarena.td_ready_o = taking_c && !pv_valid_i`, and
`taking_c` is `sink_c || engine_free_c` -- internal arena state with NO term
from this door. TD also outranks the chunk port in that arena's priority
(`ck_ready_o` is additionally qualified by `!td_valid_i`), so GEOM.ARENABIN
holding `ab_tri_ready_w` low while it writes a chunk can never starve the push
that releases the door.

### Proven -- the same fixture, the same script

```
SMOKE: tidq       underflow=0 overflow=0 unnamed=0
SMOKE: tidqids    pushed=0 1 2 3 4 5 6 7 | popped=0 1 2 3 4 5 6 7
SMOKE: tidqjoin   OK: 75 push(es), 75 pop(s), first 8 id(s) equal element by element, underflow=0
SMOKE: arenabin   tris=75 unnamed=0 refs=101
SMOKE: arenabin   AGREES WITH THE BINNER EXACTLY: 101 tile reference(s) from both, independently binned
SMOKE: raster     pixels=2816     frames_admitted=1
```

**popped[k] == pushed[k].** The external arena's tile lists are no longer four
references short of the picture; the two independent producers agree exactly.

### The deadlock mode is EXERCISED, not argued

`tests/geometry/geom_tidq_directed.cpp` -- **written by this packet because it
did not exist** (section 6) -- 83 checks, BUILT AND RUN per ruling R60.

* it **fires all three counters** with legal stimulus:
  `underflow=1 overflow=2 unnamed=1`, retiring the module header's
  never-demonstrated claim that they were reachable;
* **case 2 reproduces the ungated skew exactly**:
  `pushed=100..105 | popped=262143 100 101 102 103 104 105`;
* **case 3** runs the same stimulus through the occupancy gate, offering the
  door five beats before the first id exists -- more skew than GEOM.SETUP's
  three stages can produce -- and requires popped[k] == pushed[k];
* **case 5 is the deadlock mode**: a flush with three entries still owed. The
  requirement is stated as DELIVERY (the gated door still gets three beats),
  because against the old RTL it fails by the queue going EMPTY, which IS the
  stall;
* **case 6** is the push on the flush clock -- the seal-abort entry;
* **case 7 is the negative control for 5 and 6**: an id pushed after the flush
  must come out NAMED. A latched poison would pass every other check in the
  file and render an empty console from frame two.

**THE CONTROL WAS SHOWN TO FAIL.** Built against
`0cc81ee5:fpga/rtl/geometry/zhao_geom_tidq.sv`, the identical 83 checks report
**21 FAILURES**, all in cases 5/6/7. Cases 1-4 and 8 pass against the old RTL,
which is correct and is the point: the queue's DATA PATH was always sound and
only its frame-edge semantics were not.

**The smoke's assertion asserts the CORRECT behaviour, never the bug** --
underflow zero, the streams the same length, the streams equal element by
element. "The underflow counter fires" would have passed only while the defect
existed. ARENAINFER's decision not to assert is overturned on its own stated
condition: *"a decision the coordinator should overturn if the defect is not
being taken immediately."* It was taken.

### The smoke assertion CAN FAIL, and its positive control is already measured

The brief asks that any control added be checked for vacuity. This one does not
need a mutant or a guard, because **its positive control is the base commit's
own captured output**: at `0cc81ee5` the identical three checks would have
fired on the first one tested -- `geom_tidq_underflow_o` read 1, and
`popped[0] = 262143` against `pushed[0] = 0`. The log is
`gz-rasterswap-smoke-base.log` and it was taken before any edit.

The element-by-element loop is the only part that could go vacuous, if a future
fixture drew no triangles at all. It prints the number of pairs it compared
(`first 8 id(s) equal element by element`) rather than leaving that to be
inferred, and the other two checks -- underflow zero and pushes == pops -- are
non-vacuous at any scene size.

---

## 3. THE SECOND SETUP BACK END -- NOTHING WAS BUILT, DELIBERATELY

See section 7. The short form: blocker 2 is untouched and is still 1,749 bits,
and building it is pure ADDITION on the subsystem the budget is tightest on,
ahead of a storage re-architecture that entry I55 already names as the next
experiment.

**What this packet DID move toward it, and it is a real precondition nobody
knew was unmet:** until today a walk over the external arena would have
followed chunk lists in which **99% of the triangle ids named the wrong
triangle**. The arena's contents are now correctly named end to end and agree
exactly with the picture the raster draws. Directive section 4's requirement
that a frame's output *"depends on the bytes written and read through the real
guard/arbiter/controller path"* was unreachable before this commit for a reason
that had nothing to do with the back end -- the bytes were there, and they
named the wrong geometry.

### Why NO PART of it was built standalone either, which is the option I
### actually weighed

CLAUDE.md says to build new blocks standalone and wire several in at once, and
the obvious bounded start is the vertex-fetch arm `zhao_geom_paramwalk`
deliberately does not drive. **I considered it and rejected it, on the
campaign's own accounting.**

A new RTL module that nothing instantiates is exactly the BUILT, INSTALLED
NOWHERE shape `tools/budget/uncashed_cheques.py` exists to detect. It would
need a disposition in `design/console_inventory.yml` to pass gate G4, and the
only honest one is `pending_compose` -- which that file's own rule calls a WORK
LIST that "must never become comfortable". So the move would convert a clean
`BUILT BUT NOT CONNECTED: 0` into a declared deferral, while the register's
headline number sat still and a reader saw progress.

**The swap is all-or-nothing by this campaign's own metric**, and the honest
sequence is: resolve GEOM.ARENABIN's storage inference first (it is the named
next experiment and it is an 87%-of-device problem), then build the back end as
one subsystem and spend one fit on it. That is a plan, not a refusal, and it is
written here so the next packet inherits the ordering rather than the argument.

---

## 4. `paramwalk` DID NOT MOVE OFF ZERO, AND THAT IS REFUSED AGAIN

```
SMOKE: paramwalk  dirs=0 dirmiss=0 chunks=0 stale=0 illegal=0 depth=0
SMOKE: paramwalk  tris=0 trisbad=0 cut=0 denied=0 short=0 stray=0 genrace=0
```

Unchanged, and refused for the third time for the reason BINARENA and
ARENACOMPOSE refused it: **every `t_*` output dangles**, so wiring
`walk_valid_i` to a tile sequencer would count triangles and drop them. That is
logic added to make a counter move. With a real consumer on `t_*` it is no
longer a demonstration -- it IS the raster swap, and section 7 is its price.

---

## 5. THE PRICE, RE-MEASURED AT THIS COMMIT

### The swap's own price, RE-RUN rather than quoted

`geom_arenabin_price` had to be re-run and not cited: the bench top
`tests/geometry/tb_zhao_geom_paramarena.sv` INSTANTIATES `zhao_geom_tidq`, so
it is inside this packet's change closure. Quoting entry I55's numbers would
have been describing a tree that no longer exists.

Built standalone at this commit with HAVE_ONCHIP=1 -- the whole legacy path
compiled in and CLOCKED -- **549 checks, 0 failures**:

```
PRICE on-chip drain:  refs=27  span=107 clocks            ->  4.12 clocks/ref
PRICE external walk:  tris=19  busy=562 clocks  reqs=22   -> 29.58 clocks/tri
PRICE producer:       fixed=2176 clocks/frame  marginal=5.82 clocks/ref
PRICE producer, AT R7's GIANT (32768 refs):                   5.88 clocks/ref
PRICE producer backpressure: small=14  large=8093 clocks
```

**Byte-identical to entry I55's figures. The consumer side is still 7.18x**
(29.58 against 4.12) and the repair did not move it in either direction --
which is the expected result and is worth stating as a measurement rather than
as an assumption, because the queue is in that binary.

### The console-side cost of the door gate, and one counter that lies

`vertid stall` reads **1774 before and 0 after**. **That is not 1,774 clocks
saved.** The wait MOVED: with the door gated, GEOM.SETUP's `pipe_en` drops and
backpressure now reaches GEOM.CLIP *before* the fork, so the same wait is no
longer counted at GEOM.VERTID's input. `vid_stall_o` counts
`tri_valid_i && !tri_ready_o`, and that `tri_valid_i` is itself qualified by
GEOM.SETUP's ready -- so the change moved one of the counter's own operands.
Quoting it as a speed-up would be this campaign's "measured refusals and called
it a 62x speed-up" in new clothes.

The honest whole-frame number is SDRAM occupancy over the same fixture:

| | base `0cc81ee5` | repaired |
|---|---|---|
| `sdram_busy` | 645,849 (88%) | 645,846 (88%) |
| `bursts [e0 rd/wr, other]` | 11520/23392, 18577 | 11520/23392, 18577 |
| `raster pixels` | 2,816 | 2,816 |
| `arenabin stall` | 0 | 0 |
| `paramarena verts/tris/chunks` | 213 / 75 / 15 | 213 / 75 / 15 |

**Three clocks in ~730,000.** The gate costs nothing, and the reason is
structural rather than lucky: `zhao_geom_vertid.tri_ready_o` is
`(st_q == S_IDLE)`, so the fork already could not hand over triangle N+1 until
VERTID finished N -- the same event that pushes N's id. The door's wait is time
the fork was already spending.

---

## 6. CLAIMS IN THE BRIEF OR THE ENTRY FOUND FALSE

**1. `geom_tidq_directed` DOES NOT EXIST -- and it is cited TWICE as passing.**
This is the campaign's false-PRESENCE shape, and it is the more dangerous one.

* `fpga/rtl/prod/zhao_console_core.sv:20030` -- *"It survived because the block
  was right and the COMPOSITION was wrong: `geom_tidq_directed` drives a real
  clock and passes"*
* `reports/HANDOVER-20260919.md:3159` -- the same sentence

`zhao_geom_tidq` had **exactly one test in the whole tree: `lint_geom_tidq`**.
A reader who greps the name finds a citation and stops. It is written now, and
both sentences are corrected in the same commits that make them true.

**2. The module header's counter claim had never been demonstrated.**
`zhao_geom_tidq.sv` asserted *"Both counters ARE REACHABLE WITH LEGAL STIMULUS
... so neither owes a committed mutant"* since entry I54. True, as it turns out
-- but it was an ARGUMENT, and under this campaign's own law a detector that
has not been seen to fire is a claim. All three now fire in a committed test,
and the header says which test rather than asserting the property.

**3. The brief's costed repair is right in all three parts, and understates
one.** *"That alone can DEADLOCK"* is correct about the seal abort. It is ALSO
true of the FLUSH by itself, with no abort involved: the flush discarded
entries owed to triangles still inside GEOM.SETUP, which strands the gated door
even on a console where `vid_seal_abort` is 0 forever -- which is every form
measured here. The brief folds the two together (*"the flush and the abort
coincide by construction"*); they coincide in TIME but they are two independent
ways to strand the door, and repairing only the abort would have shipped a live
deadlock. `geom_tidq_directed` case 5 exercises the flush case with no abort at
all, and it is the case that fails against the base RTL.

**4. Entry I55's own framing of `unnamed=1` is still the first thing a reader
meets and is still misleading.** ARENAINFER corrected it in FINDINGS; the entry
text continues to present it as a seal-ordering question and as *"four
references short of the picture"*. The four references were the visible part.
The invisible part was 74 triangles carrying a neighbour's index, and the entry
is amended here rather than left for a third packet to re-derive.

---

## 7. WHAT WAS REFUSED

**THE RASTER SWAP ITSELF.**

* **Blocker 2 is untouched and is still 1,749 bits.** `job_*` needs
  METAW = 1877; `t_*` is one 16-byte TriangleDescriptor decoded -- 128 bits.
  The difference is six 240-bit plane equations, `tri_area2_i`, `tri_min_x_i`,
  the 298-bit flat request, the 48-bit continuation tail and the 32-bit
  fragment state, all manufactured by GEOM.SETUP and GEOM.ATTRPACK from full
  vertices. The swap owes a SECOND setup and attrpack back end fed from SDRAM
  plus the vertex-fetch arm `zhao_geom_paramwalk` deliberately does not drive.
  **That is ADDITION, not substitution**, and nothing in this packet reduces it.
* **It sits behind a prerequisite the tree already names.** Entry I55's own fit
  section records `zhao_geom_arenabin` mapping to **146,414 registers** on the
  shipping part -- about 87% of the device's register sites in one block --
  because its 145,152-bit staging array did not infer as memory. Hoisting those
  banks out of the generate is *"the next experiment"* in the entry's own
  words. Building a second geometry back end on top of an unresolved 87% would
  measure a machine nobody can ship.
* **Half-doing it remains forbidden** and was not done. ORing the walk into the
  live stream is refused for the fourth time.

**This is a BUILD refusal, not a DECISION refusal, and I say so explicitly**
because CLAUDE.md's newest chapter is about exactly that confusion. Directive
section 4 decided the question -- *"a parallel legacy on-chip frame arena that
still supplies the actual pixels is not closure"* -- and nothing here waits on
the owner. What remains is WORK: a second setup back end, a vertex-fetch arm,
and a storage re-architecture. This packet judged the join repair the
higher-value and correctly-ordered half, on the brief's own terms: *"A packet
that repairs the tidq join, proves the ids correct, and declares the swap still
open is a good packet."*

**A CONSOLE OR FULL-DEVICE FIT** -- forbidden to this packet and not run. No
`quartus_map` was run at all, so **this packet makes no area or timing claim**.
The diff adds one 4-bit register and three AND terms in `zhao_console_core`,
and one `[AW:0]` register with its decrement in `zhao_geom_tidq`. That is a
statement about the diff, not a measurement, and it is offered as such.

---

## 8. WHAT I GOT WRONG AND CAUGHT MYSELF

**1. My own directed test's summary block read `unnamed=0` and said so.** It
pushed the nameless entry AFTER filling the queue to DEPTH, so the push
overflowed and never landed -- the counter it was summarising could not move.
It failed loudly (1 of 78) rather than passing, but the shape is the
vacuous-control one: a block asserting three counters fired, one of which its
stimulus could not reach. Fixed by pushing the nameless entry first; the file
records why in a comment rather than silently.

**2. My `check()` calls printed "expected 0x1, got 0x1" ON FAILURE.** I passed
literals for the expected/actual arguments of every `pop_if_ready` assertion,
so the negative control's first run produced fourteen failure lines whose
numbers claimed the values agreed. That is a misleading instrument inside the
instrument I was building to catch misleading instruments. Every one now passes
the real boolean, and the negative control was re-run to confirm the numbers
read honestly.

**3. My first plan for the repair was too narrow.** I intended to gate only
`door_tri_valid_w` -- the obvious single edit, and what the brief's one-line
form (`door_tri_valid_w &&= (tidq level != 0)`) suggests. Reading the three
consumers first showed that `st_o_ready` and GEOM.ATTRPACK's `out_ready_i` are
NOT functions of `door_tri_valid_w`, so that edit alone would have let
GEOM.SETUP and GEOM.ATTRPACK retire a triangle on a clock the door did not
fire -- converting a MISNAMED triangle into a LOST one, which is worse. Caught
by tracing the consumers before editing, not by a test.

**4. A process error worth recording for the next packet.** I launched three
control forms with PowerShell `Start-Job`. Each PowerShell tool invocation is
its own session, so `Get-Job` in the next call returns NOTHING and the jobs are
invisible -- which reads exactly like "they died". Two had in fact survived and
one had not. The tell is the log file, not the job table: check whether the
output file is growing before concluding anything about a background run.

---

## 9. BRANCH AND COMMITS

**Branch `gz/rasterswap`. Pushed. Never `--force`, never `--force-with-lease`.**

| commit | what |
|---|---|
| `85ffc6df` | the join repair: `zhao_geom_tidq` flush semantics, the console door gate, the seal-abort push, `tests/geometry/geom_tidq_directed.cpp` (new, registered), the smoke's `tidqjoin` assertion |
| `52b7743c` | entry I54 and I55 amended with the measurement; the false `geom_tidq_directed` citation corrected in `zhao_console_core.sv` AND `reports/HANDOVER-20260919.md` |

### Gate state at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** -- no higher than when I started (RC 1 is normal), and **still 2 after the entry-text edits**, so no free reduction by reformatting |
| `check_console_inventory.py` | RC 0 |
| `check_prod_manifest.py` | RC 0 |
| `gen_prod_top.py --check` | fresh (88 instances) |
| `gen_console_board.py --check` | RC 0 |
| `mutant_copy_drift.py` | OK, 78 copies -- **run AFTER the commit** (ruling R121) |
| `check_quartus17_syntax.py` | RC 0 |
| `gen_shell_paired_diff.py --check` | harness fresh, mutant fresh |
| `check_case_labels.py` | RC 0 |
| `check_console_closure_lint.py` (GATE 31) | OK, 294 sources, **self-test fired 5/5** |
| `check_entry_claims.py` | RC 0, no new claim that a composed module is uncomposed |
| `zhao_geom_tidq` lint, `-Wall` explicit | RC 0, zero diagnostics |
| `npm run abi:check` | not run -- `spec/commands.zidl` untouched |

`check_quartus17_syntax.py` at RC 0 settles ONE tool's opinion. **No block here
went through `quartus_map` in this packet**, and a clean lint is not evidence
about synthesizability.

### Directed tests, BUILT AND RUN (ruling R60)

| test | result |
|---|---|
| `geom_tidq_directed` (NEW) | **83 checks, 0 failures**; counters fired `underflow=1 overflow=2 unnamed=1` |
| `geom_tidq_directed` vs base RTL | **21 of 83 FAIL** -- the negative control fires, in cases 5/6/7 only |
| `geom_arenabin_price` (HAVE_ONCHIP=1) | **549 checks, 0 failures**; price unmoved |
| `geom_arenabin_directed` (HAVE_ONCHIP=0) | **317 checks, 0 failures** -- the independent producer's round trip through the real guard, arbiter, controller and SDRAM and back out through the real `zhao_geom_paramwalk` is undisturbed |

Neither could be run through `ctest`: a fresh worktree's `cmake --preset`
configure verilates ~256 targets. Both were built standalone with the recipe
`tests/prod/run_console_core_smoke.ps1` already encodes (no `make` on this box,
so `--cc --exe` writes the unit list and the compile and link are done by
hand). The CMake registration is committed and correct; the standalone build
scripts are scratch and are not.

---

## THE SMOKE FORMS -- ALL SEVEN, AND THE JOIN IS CORRECT IN EVERY ONE

Every committed form was run against the pushed commit. `tidqjoin` is the new
assertion; `arenabin` is the total agreement check between the two independent
producers, which could not have read EXACTLY before this repair -- the external
arena was short by the references of every triangle whose identity was wrong.

| form | verdict | `raster pixels` | `tidqjoin` | `arenabin` vs binner |
|---|---|---|---|---|
| plain | PASS | **2,816** (gate row), `frames_admitted=1` | 75 / 75, ids equal | **AGREES EXACTLY: 101** |
| `-Mutant` | PASS -- inverted control FIRED (`terr_pl_slot_overflow_o`=1) | 2,560 | 14 / 14, ids equal | **AGREES EXACTLY: 36** |
| `-BadVertex` | PASS | **512** -- the terrain-only number this form reads | 61 / 61, ids equal | **AGREES EXACTLY: 65** |
| `-NoEchoArm` | PASS | 2,816 | 75 / 75, ids equal | **AGREES EXACTLY: 101** |
| `-BadTraceArm` | PASS | 2,816 | 75 / 75, ids equal | **AGREES EXACTLY: 101** |
| `-TerrainFlatLattice` | PASS | **2,560** -- the mesh-only number this form reads | 14 / 14, ids equal | **AGREES EXACTLY: 36** |
| `-BadDescriptor` | PASS -- inverted: `SMOKE_RC=1` is the required outcome | 2,560 | 34 / 34, ids equal | **AGREES EXACTLY: 46** |

In EVERY form `tidq underflow=0 overflow=0 unnamed=0` and `tidqids` reads
`pushed == popped` element by element, at **five different scene sizes**
(75, 61, 34, 14, 14 triangles). That is what says the repair is a HANDSHAKE and
not a constant tuned to one fixture.

**And the new assertion does not mask anything.** `-BadDescriptor` is an
INVERTED control that must FAIL, and it still failed for its own reason, with
the join reading correct beside it. A new `$fatal` that pre-empted a control's
intended failure would have turned a positive control into a false green; this
one was checked for exactly that.
