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
