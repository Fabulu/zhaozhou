# Task Log: RUN-20260824-0522 - projector merge, phase 1

**Created:** 2026-08-24 05:22 UTC+02:00
**Status:** Active
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260824-0522-projector-merge-phase1/

---

## Objective

`zhao_geom_project` and `zhao_terrain_project` implement the same projection
law twice, at 33 DSPs each. Extract one shared projection core and make both
blocks use it, without changing either caller's behaviour, timing or ordering.

---

## Progress Timeline

### 05:22 - Task started

Run initialised with `runs\CLAUDE-RUNS\init-run.ps1 projector-merge-phase1`.
Read RUN-20260824-0317 first, as instructed; its nine-item failure list and its
Don't-Retry entries are inherited into this run's SPEC verbatim rather than
paraphrased.

### 05:23 - The two-git trap, re-confirmed before it could cost anything

`git status --short` at the same moment on the same tree:

| shell | lines reported |
| --- | ---: |
| PowerShell tool (git 2.49.0, `core.autocrlf` unset -> false) | **294** |
| Bash tool (Git for Windows 2.45.2, `core.autocrlf=true`) | **0** |

Identical to RUN-20260824-0317 §03:20 (which measured 291). All 294 are
line-ending phantoms. **Ruling for this run, inherited: all git goes through
Git Bash.** I hit this in my first two tool calls; reading the prior run first
is the only reason it cost nothing.

### 05:25 - Baseline taken BEFORE touching anything

`ctest --test-dir build -L fast` -> **271 of 272 passed, 1,220 s**, the single
failure being

    ledger_check: V16: FIELD.SEQ.CORE is RTL_VERIFIED but formal
    "tests/formal/field_seq_bound.sby" is recorded as "pending"

byte-identical to the baseline RUN-20260824-0317 and RUN-20260823-1736 both
recorded. Pre-existing, another lane's gate, out of scope. Evidence:
`baseline_ctest_fast.log`.

### 05:26 - Step 1: the oracle resolves, and both blocks agree on the law (V17)

Required before writing any RTL. Both blocks declare the same
`reference_model: zref::render::project_vertex` in `design/blocks.yml`
(GEOM.PROJECT line 2055, TERRAIN.PROJECT in the same shape), and that symbol is
defined once, at `reference/src/zrender/rast.cpp:43`. **They do not merely cite
the same name; they cite the same nineteen lines.**

Checked step by step against both headers rather than by name-matching:

| step | oracle | `zhao_geom_project` | `zhao_terrain_project` |
| --- | --- | --- | --- |
| row sum | `mat4_vec4`, exact s128, ONE rescale | `ROW_W = 68`, one `rescale16_row` | identical |
| near plane | `clip.w.raw <= 0` -> default `ProjOut` | `pre_behind = (s2_cw <= 0)` | identical |
| ndc | `fx_div_exact` | 31-step restoring recurrence | identical |
| screen | `fx_mad(ndc, hw, cx)` | `MAD_W = 64`, one `rescale16_mad` | identical |
| px | `to_screen_xy` + clamp +-2048 px | `to_screen_xy` | identical |
| depth | `fx_div_exact(1<<16, clip.w)` | lane 2, numerator `48'h0001_0000_0000` | identical |

`zhao_geom_project`'s header records that the ledger originally declared its
oracle as `zref::GeomProject`, one of the twenty-five phantoms in
`reports/PHANTOM_REFERENCES.md`; that citation has already been corrected to
`project_vertex` and the ledger agrees today. **The oracle resolves. No
divergence in the law between the two blocks.**

A source read of the two files also finds the following BYTE-IDENTICAL between
them, modulo whitespace: `ROW_W`/`MAD_W`/`DIV_STEPS`; all eight helper
functions (`mul32`, `ext64`, `ext32r`, `ext32m`, `rescale16_row`,
`rescale16_mad`, `to_screen_xy`, `mag32`); the whole configuration register
file and its address decode; the divider setup block; the 31-stage generate;
and the quotient assembly. That is the shared core, already written twice.

### 05:29 - Step 2: the equivalence differential, before any RTL

`pair_equivalence.cpp` + `run_pair_equivalence.sh` + `apply_probe.py`.

Design note on what the differential can honestly claim. The two blocks do NOT
have the same interface — GEOM takes one vertex per handshake (contract
latency **fixed 36**), TERRAIN takes one triangle and walks three corners
(contract latency **fixed 38**, two more stages: a vertex sequencer in front
and a reassembly register behind). A port-by-port cycle comparison **between
them** is therefore not defined, and asserting one would be inventing a
requirement the design deliberately does not have. So:

* **Between the two blocks**, the compared quantity is the projected vertex —
  canvas x, canvas y, the Q16.16 1/w word, the behind-the-eye flag — for every
  vertex of every triangle, in order, no tolerance.
* **Timing and ordering** are held separately, per caller, against that
  caller's own pre-change self (`caller_regression.cpp`, step 3). That is the
  comparison that can actually detect the failure the brief warns about.
* **The oracle is the third leg.** Two blocks agreeing with each other and both
  being wrong is a real failure mode; every vertex is checked against
  `project_vertex` as well, and all three must agree.

Eleven phases: a Duo camera pair, deliberately asymmetric viewports, a matrix
tuned so `clip.w <= 0` fires on a large fraction of vertices, guard-band rails,
each with and without a rotating consumer stall — then three phases that
**reconfigure both views without an intervening reset**, which is the only
place a stale-matrix or latched-configuration defect can show.

Stimulus is generated ONCE into a `std::vector<Tri>` before either DUT is
touched, and both are driven from that same vector. This is
RUN-20260824-0317's failure 1 designed out rather than avoided by care: that
run called `rnd()` inside a lambda applied to both models, the RNG advanced
twice per cycle, the two DUTs got different stimulus, and all 624 reported
"mismatches" were the harness comparing two experiments.

### 05:31 - A build failure reported itself as a finding

First run printed:

    RESULT: the two shipped projectors are NOT equivalent. STOP.

**It had not built.** `link_and_run` returns 2 when the executable is missing
and the caller tested only `-ne 0`, so a missing generated header —
`zhao_abi.h`, which lives in `runtime/include`, not under `reference/` —
came out as a verdict about the RTL. Fixed by separating rc 2 from rc 1 in both
the real-pair path and the control loop, and by aborting loudly on it.

Recorded because it is exactly the shape of error this project keeps paying
for: **a tool that cannot run must say so, not return the alarming answer.**
Had I not read the log and simply believed the line, this run would have
stopped and reported a nonexistent divergence between two blocks.

### 05:44 - The differential found a gap in ITSELF before it found anything else

First scored run: **the real pair EQUIVALENT over 12,300 vertices, and 9 of 10
controls caught.** The one that was not:

    P6 terrain: the near plane becomes strict (w == 0 moves to the accept side)
        *** NOT CAUGHT -- the differential is blind to it ***

The law is `clip.w <= 0`, and `w == 0` exactly belongs on the REJECT side --
it is law 2 in `geom_project_directed.cpp`'s own list. Eleven random phases and
12,300 vertices never once produced a `clip.w` of exactly zero, which is not
surprising: it is one word out of 2^32 per vertex.

**This is the difference between a differential that passes and a differential
that means something.** Had the control table stopped at the five defect
classes the brief names, all five would have been caught, the pair would have
been declared equivalent, and the harness would have been silently blind to the
one boundary the law is actually written about.

Closed with a test rather than argued (the brief's rule, and the right one). A
new phase configures a matrix whose w row is the IDENTITY ON X -- `m[3][0] =
1.0`, every other w-row word zero -- so that

    row_cw = 1.0 * x   ->   rescale(.,16) of it is the raw word x itself

and `clip.w == x` EXACTLY. Then a full sweep of x over {-3..3} on all three
corners, both views (view 1 uses -1.0 so the sweep straddles the boundary from
the other side), 2,058 vertices, no randomness anywhere near the boundary.

### 05:52 - STEP 2 RESULT: the two blocks ARE equivalent

    16,416 vertices compared on three-way agreement, 0 mismatches
    RESULT: geom_project, terrain_project and project_vertex AGREE EVERYWHERE.
    controls: 10 caught, 0 BLIND, of 10

Thirteen phases: the exact near-plane boundary (with and without stalls), a Duo
camera pair, deliberately asymmetric viewports, a `clip.w <= 0`-dominated
matrix, both guard-band rails, each with and without a rotating consumer stall,
and three phases that reconfigure both views WITHOUT an intervening reset.

The ten controls, every one caught, with the mismatch count each produced:

| control | caught |
| --- | ---: |
| P1 terrain: view tag swapped at the viewport stage | 5,234 |
| P2 terrain: a config write lands in the wrong view's matrix | 44,495 |
| P3 geom: viewport transform uses the other view's viewport | 2,054 |
| P4 terrain: triangle reassembled out of order (B takes C's word) | 3,418 |
| P5 geom: a stale matrix -- row 0 uses view 0 regardless of the packet | 2,712 |
| P6 terrain: near plane becomes strict (w == 0 to the accept side) | 2,352 |
| P7 geom: guard band clamps one count short of the rail | 4,680 |
| P8 terrain: row sum rounds toward zero instead of half up | 1,611 |
| P9 geom: the 1/w lane divides the wrong numerator | 8,325 |
| P10 terrain: behind-the-eye vertex keeps its coordinates | 6,915 |

P1-P5 are the five defect classes the brief names for the mutation sweep,
submitted to the differential first so that the sweep is not the only thing
standing between the merge and them.

**The audit's "byte-identical arithmetic signatures" is now backed by a
behavioural claim rather than standing on a census coincidence.** The merge may
proceed.

### 06:05 - Step 3: the core, and where its boundary had to fall

`fpga/rtl/common/zhao_project_core.sv`. Both shells instantiate it. 1,395 lines
of RTL become 1,126.

**The seam is placed by the two latency numbers, not by taste**, and this was
the one real design decision in the change. GEOM.PROJECT's contract says fixed
**36**; TERRAIN.PROJECT's says fixed **38**. So the core ends at its
`to_screen_xy` register, because that register is simultaneously

  * GEOM's output register -- so GEOM adds no stage, and
  * TERRAIN's old `s6` -- so TERRAIN keeps its sequencer in front and its
    reassembly register behind, and adds exactly the two stages it always had.

One stage earlier would have forced GEOM to add a stage; one later would have
cost TERRAIN one. There is exactly one admissible boundary and the two contracts
pick it between them.

**The enable is the CALLER'S, not the core's.** The two callers back-pressure
from different places: GEOM from the core's own output register, TERRAIN from the
triangle register one stage further on. The pre-merge blocks each computed
`advance` locally and they are NOT the same expression. A core that derived its
own would have had to pick one of the two and silently change the other -- which
is precisely the failure the brief warned about, so it is designed out rather
than tested for. `en_i` is an input.

`view` is a first-class core signal, not payload: it selects the matrix at
stage 1 and the viewport at stage 6. The riders are an opaque `PAYLOAD_W` word
the core never interprets -- 16 bits of `src_id` for GEOM, 42 bits of
`{corner, src, matA, matB, weight}` for TERRAIN.

Both lint clean at `-Wall`. All nine existing lanes pass, including the nightly
randoms and `terrain_project_chain`, which composes through GEOM.CLIP, SETUP and
BINNER.

### 06:12 - Step 3 evidence: nothing about either caller moved

`caller_regression.cpp` runs each rewritten shell beside a VERBATIM copy of its
own pre-merge self, recovered with `git show` (never `git checkout <rev> --`,
which stages), and compares **every output port on every cycle**.

    80,000 cycles driven, 1,080,000 port-cycles compared, 0 mismatches
    RESULT: NEITHER CALLER'S TIMING OR ORDERING CHANGED.
    timing controls: 7 caught, 0 BLIND, of 7

The stimulus is a pure function of the cycle number and deliberately does NOT
advance on `valid && ready`. `ready` is one of the outputs under test: a
queue-driven harness would feed the two models different stimulus the instant
they disagreed, and the comparison would stop being one. That is
RUN-20260824-0317's failure 1 in a subtler dress -- there the divergence came
from an RNG called twice, here it would come from feedback. It also makes the
check STRONGER than a protocol-respecting one, because it drives `valid` held
through a stall, `valid` dropped mid-handshake, and configuration written while
vertices are in flight.

**One control was withdrawn, and the reason is worth more than the control.**
T6 first mutated the counter's `accept` to `v_valid_i` and came back NOT CAUGHT.
That is not a gap. Inside `else if (advance)`, `v_ready_o` IS `advance` and is
therefore 1, so

    accept = v_valid_i && v_ready_o   ==   v_valid_i

exactly. A **provable no-op**, which no test can distinguish -- a badly chosen
control, not a blind harness. Replaced by one that removes the `advance` guard
so the counter ticks on frozen cycles; that is caught, 7,960 mismatches. The
distinction matters because "NOT CAUGHT" and "cannot be caught" look identical
in a log and mean opposite things.

### 06:30 - Step 4: MEASURED. And the brief's 66 -> 33 does not follow.

Map-only, Quartus 17.0.2, 5CSEBA6U23I7, **one job at a time, nothing else
running, against COMMITTED RTL** at b8aeeeb. `Get-Process` checked empty first.

| module | before (7395d793) | after (b8aeeeb) |
| --- | --- | --- |
| `zhao_geom_project` | 33 DSP, 5,956 reg, 5,028 ALM | **33 / 5,956 / 5,028** |
| `zhao_terrain_project` | 33 DSP, 6,685 reg, 5,503 ALM | **33 / 6,685 / 5,503** |
| `zhao_project_core` | -- | 33 DSP, 5,925 reg, 4,996 ALM |

Every number for both shells is **identical to the unit** -- DSPs, registers and
estimated ALMs alike. Quartus synthesised structurally the same hardware from
the extracted form, which is independent corroboration of the cycle-exact
regression rather than a second happy accident.

**And the pair is still 66 DSPs.** A module that two blocks INSTANTIATE is not a
module they SHARE; each shell holds its own core. The extraction bought
maintenance, not silicon, and no amount of care in writing it could have bought
otherwise.

`design/contracts/GEOM.PROJECT.md`'s own Follow-up asserted both halves of a
contradiction in one sentence -- "have both instantiate it" AND "that halves the
divider cost" -- and the brief inherited the second half. **The core's own
measured row is what settles which is true: ONE instance is 33.** Reaching 33
for the pair needs one arbitrated instance serving both callers, which is an
architecture change and not a refactor: each caller becomes stallable by the
other, and the aggregate rate halves from two vertices per clock to one.

I did NOT build that, and the reason is a number, not a preference -- see 06:45.

The shell overheads fall out of the same three rows and they are the right
shape: GEOM adds **31** registers over the bare core (its 32-bit accepted-vertex
counter) and TERRAIN adds **760** (the sequencer's nine 32-bit corner words plus
riders, and the triangle reassembly register).

### 06:45 - Why one shared instance is not affordable YET, and a demand figure wrong by 6,144x

Costing the single-instance option against `design/budgets/workloads.yml` turned
up a defect in the budget model that is independent of this merge and larger
than it.

    zhao_geom_project     120,000 vertices/frame       =  7.2% of 1,666,667
    zhao_terrain_project  270 patches x 6,144          = 99.5% of 1,666,667
                                                         -----
    one shared core                                     106.7% of a frame

`workloads.yml` already records the geom half of that argument verbatim --
"120,000 vertices at one per clock is 7.2% of the frame, so a SHARED projector
is affordable and two are not justified by rate" -- but its terrain row is:

    unit: patches
    itemsPerFrame: 270
    verticesPerItem: 3

and `tools/budget/build_manifest.py:300` computes
`demandRatio = itemsPerFrame / capacity`. **`verticesPerItem` is set on that row
and read by nothing.** So the model costs 270 *patches* as 270 *projections*:

| | demand | over-provision |
| --- | ---: | ---: |
| `reports/BUDGET_HEATMAP.md` today | 0.00016x | **6173x** |
| corrected | **0.995x** | **1.005x** |

**And it is worse than mis-scaled: 270 is a CEILING that was filed as a DEMAND.**
`design/contracts/TERRAIN.PROJECT.md:198` derives it in the sentence after the
6,144: "At 100 MHz / 60 Hz (1.67 M clocks) that is about 270 patches per frame of
pure projection". 270 is `1,666,667 / 6,144` -- the count at which the block is
exactly 100% busy. The corrected ratio is **1.0 by the construction of the
figure**.

Consequences, and the middle one is the dangerous one:

1. The heatmap ranks `zhao_terrain_project` the **most over-provisioned block in
   the design**. It is the **tightest**, and it is over its own declared
   `reserve: 0.20` before anything else runs.
2. The heatmap derives "est. DSP after: 3" for it from 6,173x of serialisation
   headroom that does not exist. **Serialising it even 3x would take it to 3x a
   frame.** A block ranked as the second-largest available DSP win is in truth
   the one block on that list that must NOT be serialised.
3. `verticesPerItem` looks like it is doing work and is not -- the same "two
   statements of one fact, and the one that is not exercised rots" shape
   `tests/lint/source_list_parity.cmake` was written about.

**Not corrected here.** Which number should move -- the row, the tool, or the
ruled 270 patches, which the corrected arithmetic now puts at 99.5% of a frame --
is the owner's call, and this run was not measuring that block's workload.
Docketed with the full argument.

The recommendation that follows: **the projected-vertex cache first.** With it,
terrain drops to 270 x 1,089 = 294,030 = 17.6% of a frame, geom plus terrain on
one core is 24.8%, and the single shared instance becomes comfortable. The
docket's existing order already puts the cache at item 7 and already says of the
projector "do NOT serialize it first" -- and sharing one instance between two
callers IS serializing them against each other, so that ruling covers this case
too. Sequence: **cache, then one shared instance, then width narrowing.**

### 07:05 - Step 4: the mutation sweep

`tools/sweep_project_core.sh` + `tools/sweep_project_core_preflight.py` +
`tools/sweep_apply_mutant.py`. 23 mutants over all three files, run **detached in
a git worktree at the shipping commit with its own build directory**
(`/c/programmieren/zencrifice/.pcsweep`, `build-pcsweep`) -- the standing owner
ruling, and also what let the main tree carry the documentation edits while the
sweep was alive. Guards 1-7 carried unchanged from `sweep_surface_sheet.sh`.
Liveness confirmed with `Get-Process`, not by the absence of an error: a stopped
background task is not a stopped process, and RUN-20260824-0317's ninth failure
was exactly this uncertainty.

Two additions to the inherited pattern, both of which are gaps in every existing
sweep:

* **Every mutant is linted as FOUR tops** -- both shells plus the core standalone
  at `PAYLOAD_W` 16 and 42, the two widths its callers actually use. A mutation
  in the core changes both shells, so linting only the edited file would
  reproduce, in the tooling, exactly the blind spot this run existed to remove.
  A parameter with one tested value is a constant with extra steps.
* **GUARD 8: the random lanes are RUN.** Every sweep in this tree invokes each
  consumer exe bare. ctest also invokes `test_geom_project_directed` as
  `--random 2000` and `test_terrain_project_random` as `--nightly`. Scoring a
  mutant against a strictly smaller test set than CI applies overstates the
  suite, silently and in the flattering direction.

The sweep also cross-checks that **the core's consumer set is the UNION of the
two shells'**, and aborts if it is not: if the extraction had failed to land in
both callers, every core mutant would be scored against a smaller set than it
reaches, and the score would look fine.

**The preflight earned its keep on the first run**: 23 parsed (non-zero,
verified), 1 rejected --

    M23 TERRAIN: idle_o ignores the core
        zhao_terrain_project: %Warning-UNUSEDSIGNAL ...:254:21

Removing `core_busy` from `idle_o` ORPHANS it. That is a build failure wearing a
defect's name, and three runs of this project's history have such things scoring
as CAUGHT. **The mutation was rewritten, not the guard**: dropping `out_valid_r`
instead is a real defect -- `idle_o` asserting while a finished triangle still
waits at the output -- and orphans nothing, because `out_valid_r` also drives
`out_valid_o`.

*(continued)*

---

## WHAT DID NOT WORK

*(see the closing section; kept contemporaneous)*

1. **A build failure reported itself as "NOT equivalent".** §05:31 above. The
   harness's own error path produced the most alarming possible sentence about
   the RTL, from a missing include directory.
2. **A `python - <<'EOF'` heredoc failed to parse** — again, and this time it
   was my own quoting: a `\` immediately before a closing `'''` escaped the
   quote and the string never closed. My own SPEC's Don't Retry says to write
   the script to a file first; I did not follow it and lost a retry. The rule
   is in the SPEC because RUN-20260824-0317 lost two.

---

## Files Created

- `SPEC_v1.md`, `TASK_LOG.md`
- `baseline_ctest_fast.log` — 271/272, the inherited `ledger_check` failure
- `pair_equivalence.cpp`, `run_pair_equivalence.sh`, `apply_probe.py`
- `pair_equivalence.log` — 16,416 vertices, 0 mismatches, 10/10 controls

## Next Steps

- Finish step 2; read the control table before believing the verdict.
- Step 3: extract `zhao_project_core`, both callers instantiate it.
- Per-caller cycle-exact regression against pre-change RTL.
- Mutation sweep in a worktree; map; ctest; ledger; commit.
