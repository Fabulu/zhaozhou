# DSP sequencing on zhao_terrain_lod - Findings

**Agent ID:** claude-dsp-terrain-lod
**Created:** 2026-08-23
**Parent Task:** RUN-20260822-2136
**Status:** Complete — **amended 2026-08-23 after this work broke `main`.**
Findings 8-10 are the amendment and are the part worth reading first: the DSP
result below stands unchanged and was never the problem, but the GATE I reported
it behind was not a gate, and my own mutation sweep left a mutant on disk that
took `main` red.

---

## Summary

`zhao_terrain_lod`'s thirty products were sequenced through **one 32x32 unsigned
multiplier**, and the block fit measured **28 DSPs -> 3** with **ALMs also
falling, 2,086 -> 1,759**. The block went from a quarter of the device's
multipliers to 2.7% of them, for fourteen extra clocks on a path whose rate had
an 11x margin and now has 8x. The `zhao_geom_lod` pilot's lever works a second
time and works harder: 89% of the DSPs, against the pilot's 67%.

The contract's own prediction — that six of the twenty-four ladder multiplies
were constant shifts — **was real, and was worth 6 of the 28.** The other 19 came
from two things it had not noticed: twelve of the twenty-four left-hand sides
were exact duplicates, and the whole block fits in a multiplier NARROWER than
the one it was using.

**AMENDMENT.** `main` went red after this landed: `measure_governor_lod` failed
55 of 72 checks on the Duo fairness law. **It was not the RTL** — the sequenced
block passes that composition 72/72 with a trace byte-identical to the block it
replaced. It was **my mutation sweep**, which re-elaborated every consumer of the
module through `cmake` but cleaned only the two it scored, leaving mutant-derived
model sources on disk for the next build to compile. I did not catch it because
the `ctest -L fast` I reported green **had never rebuilt the composed target**.
Both failures are mine and both are written up in findings 8 and 9.

---

## Findings

### 1. The measurement, both sides, same machine

| | ALMs | DSPs | registers | fit seconds | commit | worktree |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| before | 2,086 | **28** | 1,257 | 434.3 | `47d607c` | clean |
| after | 1,759 | **3** | 1,634 | 673.2 | `9f2928f` | clean |
| delta | **-327 (-15.7%)** | **-25 (-89%)** | +377 | +239 | | |

Quartus Prime Lite 17.0.2, 5CSEBA6U23I7 (provisional), `run_block_fit.ps1`,
virtual pins, no composed fit. Device share: **25% -> 2.7%** of 112 DSPs.

The BEFORE was re-measured rather than quoted. The committed row said
2,086 / 28 / 1,257 at `d4f5bd2`; re-run at HEAD with a clean worktree it
reproduced **to the digit** (434.3 s against 404.8 s), and that row was committed
on its own (`4d37da2`) before anything changed. Unlike the pilot's baseline, this
row already carried `rtlCleanAtHead: true` — the autocrlf fix the pilot made had
taken effect — so re-measuring confirmed a good row rather than replacing a
meaningless one.

### 2. Three and not eight, and the three reasons are all reusable

`reports/DSP_Audit_2026-08-21.md` targeted 4-8 for this block. The measurement is
**3**, and it is worth being precise about where the 25 came from, because two of
the three reasons generalise and one is specific:

1. **Six were constant shifts, exactly as the contract said.** `h` is the literal
   256 in the strict ladder, so `dstv * h` is `dstv << 8`. Those six were being
   spent as DSPs only because `ladder_ok()` took `h` as an argument and the
   strict and relaxed cases shared one function. **The contract's prediction was
   correct and it was worth 6 of 28** — a fifth of the block, not the whole
   story, and the contract was careful to write it as "a measurable experiment,
   not a certainty". It was right to be.

2. **Twelve of the twenty-four left-hand sides were exact duplicates.** `s0` and
   `r0` are the same ladder with a different `h`, so they share
   `dev * cam0_scale` term for term; likewise `s1`/`r1`. Only **six** distinct
   left-hand sides exist. The parallel form wrote each one twice and the fitter
   did not fully common them.

3. **The block fits in a NARROWER multiplier than it was using.** `c` and `e`
   are both s32, so `c - e` spans `[-(2^32-1), 2^32-1]` and **|c - e| fits in 32
   UNSIGNED bits exactly**; since `d^2 = |d|^2`, the squares are a 32x32 unsigned
   multiply, not the signed 33x33 the old `sq66()` used. The ladder's 24x16 and
   32x16 products then fit inside the same operands with room over. So the ONE
   shared multiplier is smaller than any ONE of the six it replaced.

**The transferable rule is now sharper than the pilot's.** The pilot said "count
products that can reach ONE multiplier". Add: **look for constant operands and
for duplicate products before you count at all, and check whether the shared
multiplier can be narrower than the widest thing it replaces.** Two of the three
reasons above are visible by reading, not by measuring.

### 3. The ALMs went down again — twice out of two

+377 registers and -327 ALMs at the same time. Same two causes as the pilot:

* A Cyclone V ALM carries flops whether the design uses them or not.
* The area was never mostly the multipliers. It was **six parallel 66-bit
  squarers, two three-term 66-bit adder trees, and TWELVE parallel 49-bit
  comparators**. Sequencing collapsed those to one multiplier, one 66-bit
  accumulator and **two** comparators.

The comparator collapse is worth calling out separately, because it was not
forced: the products could have been computed during the root's idle cycles and
stored (six left-hand sides, 240 flops) and then compared twelve-wide. Comparing
each product **the cycle it appears** costs six extra clocks and saves ten 49-bit
comparators. That choice is why the eval phase is ordered
`rhs, lhs, lhs, lhs, rhs, lhs, lhs, lhs` rather than all products first.

**Two results out of two is a pattern, not an anecdote.** The objection that
sequencing trades area for multipliers should not be raised again on this
codebase without evidence.

### 4. What it cost, stated plainly

* **34 clocks per descriptor -> 48** (1 fill + 6 square + 32 root + 8 eval +
  1 decide). Per patch ~560 -> **~784 clocks**.
* **Margin over the ledger's rate: ~11x -> ~8x**, against
  `spec/terrain_rules.md` §4.2's 256 live/visible patches. The ledger's target
  ("1 decision per patch per frame") is still met with a wide margin, so this
  needed no owner ruling and none was invented.
* **The second camera is no longer free.** It still is in the root — the two
  lanes run concurrently — but the eval phase spends four of its eight steps on
  the second eye and the square phase three of its six. The contract now says so
  instead of leaving the old "a second camera is free" sentence to be read as
  unconditional.
* **No port changed, no handshake changed.** Unlike the pilot, this block already
  had ready/valid on both sides and `sp_ready_o` was already high only in the
  fill phase, so the callers and every test driver needed **no modification at
  all**. `tests/terrain/lod_dev.hpp` was not touched.

### 5. The verification did not weaken; it grew, and it found a real hole

**The answers did not move, and the evidence is that every coverage counter is
identical to the digit.** Random lane A still reports levels 309/857/1184/2258
with 410 coarsenings, 19 refinings, 1,412 mid-morph and 151 held back; lane B
still reports 1414/1633/1621/1732 with 468 saturations. The reference was not
touched — this was a pure hardware restructuring.

| lane | result |
| --- | --- |
| `terrain_lod_directed` | **219 checks** (was 211) |
| `terrain_lod_random` | 701 checks, both lanes' coverage identical |
| `terrain_lod_random --nightly` | **5,413 checks** |
| `terrain_lod_tess` (LOD drives real TESS) | 93 checks, still crack-free |
| `ctest -L fast` | **262/262** |
| ledger check | green, 92 blocks / 40 ops |

**THE BLOCK HAD NO MUTATION SWEEP. IT HAS ONE NOW, AND IT FOUND A HOLE ON ITS
FIRST RUN.**

First score, against the *pre*-sequencing RTL: 34 attempted / 34 accounted /
31 caught / **3 survivors**. Two were equivalent, and the sweep header proves
them rather than labelling them:

* **M11** saturates the squared distance to `2^64-2` instead of `2^64-1`. The
  only consumer is the floor root, and `floor(sqrt(2^64-2)) ==
  floor(sqrt(2^64-1)) == 4294967295` (because `4294967295^2 =
  18446744065119617025 <= 2^64-2` and `4294967296^2 = 2^64`). The rail and one
  below it are the same distance.
* **M18** targets the far edge of the hysteresis band instead of the near one.
  `want` is never used as a magnitude — only compared against `d_level`, which
  then moves by exactly one rung. Since a larger `h` makes more rungs pass,
  `t_strict <= t_relaxed` always, so swapping the edges cannot change the sign of
  either comparison. Identical packets for every input.

**The third was not equivalent.** `rhs + 1` survived all 211 directed checks and
both random lanes. The reason is exact: `terrain_lod_directed` §2 pins the flip
point with `scale = h = 256`, where **both sides of the inequality are multiples
of 256** and an off-by-one on the right is unreachable — and random input hits a
40-bit exact equality with probability 2^-40, i.e. never. §2 now builds that
equality by hand with an odd scale (`dist = 255`, `scale = 65280` and `65281`,
`dev = 1`), which is the same argument the file already made for `<` versus `<=`.
211 -> 219 checks, and the mutant was verified caught by applying it and watching
exactly those two checks go red.

Second score, against the sequenced RTL: **40 attempted / 40 accounted /
38 caught / 2 survivors** — M11 and M18, and only those two.

The sweep grew 34 -> 40 because the code moved. Ten mutants were re-aimed, and
where they landed describes the restructuring (the twelve comparisons are two;
`dev` arrives through an operand mux; the strict right-hand side is a shift;
"coarsest wins" is now "the last write in time wins"). **Six are new, and all six
are caught**: a ladder answer latched into the wrong flop, either phase ending a
step early, the accumulator not cleared between the two eyes, a right-hand side
filed under the wrong camera, and the ladder starting at level 1 instead of 0.

### 6. The sweep machinery gained a sixth guard, and it was needed

`tools/sweep_geom_lod.sh` documents five ways this build system scores a run that
never happened. This block found a **sixth**, and it is not a build-system
problem but a *transcription* problem:

> A mutation containing `$` cannot live in a double-quoted bash array.
> `zhao_terrain_lod.sv` forms its coordinate differences with `$signed(...)`, and
> inside `"..."` bash expands `$signed` to the empty string — so the anchor
> silently becomes a DIFFERENT string than the one written down.

The "source unchanged after apply" guard would catch the total-failure case, but
a *partially* expanded anchor produces a different-but-plausible mutation that
nothing checks. The mutant table therefore lives in
`tools/sweep_terrain_lod_mutants.py`, which no shell reads, and both the sweep
and the preflight import it so they cannot disagree.

**The preflight paid twice.** On the first table, three of 34 mutants failed
`-Wall` because they removed a signal's last use (`scale[15:8]`, a plain wrap of
the saturating distance, and dropping a term from a squared distance). On the
re-aimed table, three of 40 failed again — two anchored on `step` when the new
counter had been renamed `mul_step`, and one left `d_dev3` unread. **Six
malformed mutants in total, none of them ever scored.**

### 7. One RTL trap worth recording

The new schedule counter could not be called `step`: the geomorph section already
has `wire [16:0] step = morph_step_i;`. Verilator caught it as a duplicate
declaration immediately, but a bulk rename of the token `step` then rewrote
prose in comments as well. Rename the *new* identifier, not the common word.


### 8. I REPORTED A GREEN GATE THAT WAS NOT A GATE, AND main WENT RED

This is the most important finding in this file and it is not about DSPs.

**What I reported:** `ctest -L fast` 262/262, twice.
**What was true:** 262 binaries passed. Several of them had been compiled from
the *pre-change* RTL and could not have failed no matter what I wrote.

**`ctest` does not build.** It runs whatever executables are already on disk.
After changing `zhao_terrain_lod.sv` I explicitly rebuilt three targets —
`test_terrain_lod_directed`, `test_terrain_lod_random`, `test_terrain_lod_tess` —
and then ran `ctest`. **Four** targets elaborate this module. The fourth,
`test_measure_governor_lod`, was never rebuilt, so it ran a binary carrying the
old RTL and reported a pass about code that was no longer in the tree.

I compounded it by never asking who else consumed the block. My survey read
`tests/terrain/`, found three lanes, and stopped. The question that would have
caught this is one grep:

```
grep -B12 "TOP_MODULE zhao_terrain_lod" tests/CMakeLists.txt
```

which lists all four, including the composition that guards the Duo fairness
law from a different subsystem's directory.

**THE RULE, for this repo, stated so it is not re-learned:**

> A green `ctest` proves nothing about an RTL change unless a FULL build ran
> first. `verilate()` elaborates at CONFIGURE time and `ctest` has no build
> step, so `cmake -S . -B build && ninja -C build` — with **no target
> argument** — then `ctest`. Run it as the LAST action before reporting, after
> the final commit, not in the middle of the session.

That is the same family as the five build guards `tools/sweep_geom_lod.sh`
already documents. Those guards protect the *sweep* from scoring a run that
never happened. Nothing protected the *report*.

### 9. THE COMPOSED FAILURE WAS REAL, AND MY SWEEP CAUSED IT — NOT MY RTL

`measure_governor_lod` failed **55 of 72** checks on main, on charter §9's Duo
fairness property: view 1 degrading removed 96 triangles of view 0's ground.
The lead offered was that a shared multiplier needs shared sequencing state and
that some of it might not be per-view. That was a reasonable read of the
symptom. It was not the cause.

**Diagnosis, in the order it was actually established:**

1. The sequenced RTL, built clean with the target directory deleted, passes
   **72/72**. Verified three times, with the elaborated model confirmed to
   contain the sequencer (`mul_step` ×47, `lad_s0`, `rhs_rel0`).
2. The pre-sequencing RTL, built the same way, also passes 72/72 — with a
   **byte-identical trace**: `1616 / 1616 / 92 / 1514`. The composition cannot
   tell the two versions apart.
3. So the failing binary was not built from my RTL. The only thing in this
   session that put other versions of this file on disk was **my own mutation
   sweep**.
4. `cmake -S . -B build` re-elaborates **every** target that verilates the
   mutated module. The sweep deleted and rebuilt only the **two** it scored, so
   all 40 iterations left mutant-derived model sources in
   `test_measure_governor_lod.dir` and `test_terrain_lod_tess.dir`. Confirmed
   directly: after one simulated sweep iteration both unscored targets' model
   sources carry a regeneration timestamp from that iteration and contain the
   mutant.
5. **Reproduced bit for bit.** Applying **M14** — "the cameras take the coarser
   strict decision" — and building the composed test gives **55 of 72 checks
   failed** on exactly that assertion. That is the reported count, from a
   mutation whose entire purpose is to break camera isolation.

So the sweep manufactured the defect it was written to detect, and left it where
someone else's build would compile it. Fixed as **guard 7** in
`tools/sweep_terrain_lod.sh`: `TARGETS` is now the full consumer list, all four
are cleaned, rebuilt and scored, and `check_consumers` reads
`tests/CMakeLists.txt` and **refuses to start** if a consumer is missing from
it, so the next target someone adds cannot silently reintroduce this.

**A sweep must not leave the tree in a state it did not measure.**

### 10. The cross-view question, answered properly rather than by symptom

The lead deserves a real answer, because "the test passes" is not one. Enumerating
every piece of state the sequencer added, and whether it is per-view:

| state | per-view? | why it cannot leak |
| --- | --- | --- |
| `mul_a`/`mul_b`/`mul_p` | n/a | combinational, selected by `mul_step`; no memory |
| `mul_step` | n/a | a schedule index, not view data |
| `acc` (66-bit) | **shared** | the one place cross-view carry is possible — cleared at `mul_step==2` (after eye 0's three terms) and again at `mul_step==5`, before any eye-1 term is added |
| `sq_num0/sq_res0` vs `sq_num1/sq_res1` | yes | separate registers, separate root lanes |
| `rhs_rel0` vs `rhs_rel1` | yes | separate registers, written at steps 0 and 4 |
| `lad_s0`/`lad_r0` vs `lad_s1`/`lad_r1` | yes | separate registers, cleared at the start of every eval phase |

Only the *resource* is shared; every piece of *data* is per-view. The single
shared accumulator is the one real risk, and it has a mutant whose whole job is
to prove it — **M37, "the accumulator is not cleared between the two eyes"** —
which is **caught**. `M35` (a ladder answer latched into the wrong flop) and
`M39` (a right-hand side filed under the wrong camera) cover the per-view flops
from the other direction.

**No RTL change was required, and none was made.** The block at HEAD is
byte-identical to the one measured at 3 DSPs, so that measurement stands
unchanged and was not re-run — re-measuring identical RTL would produce an
identical row and a misleading second provenance entry.

**A block-level differential passing is not evidence that a composed property
still holds.** This block sits in two compositions. Both are now in the sweep.

---

## Recommendations

- **Next: `zhao_texture_tmu` (28 DSPs).** The pilot already surveyed it as the
  strongest remaining candidate, and this run sharpens the estimate. Its own
  header states its real rate is 1 sample per 4-6 clocks while the ledger still
  says 1/clock — so the parallelism is already not consumed — and **12 of its
  32 products are literal duplicates across four `zhao_texture_bilerp` instances
  driven by the same fractions**. That is exactly finding 2 above, which was
  worth twelve of the twenty-five here. Look for constant operands too: a bilerp
  weight of the form `(1-f)` is not a second multiply if the product with `f` is
  already in hand. Its contract flags the hoist as sanctioned but structurally
  invasive (ports, directed tests, formal harness), so budget for test churn
  this block did not need.
- **Then `zhao_surface_stamp` (28 DSPs), but expect less.** Its rate (1
  texel/clock) is genuinely spent on four of its six products; only the two
  radius squares are per-stamp constants latched in an already-idle acquire
  state. Sequencing those two is free and small. Sequencing the other four is a
  contract question, not a cleanup — the same shape as `zhao_terrain_project`.
- **Do not touch `zhao_terrain_project` (33 DSPs) as a restructuring.** Unchanged
  from the pilot's finding: its 33 DSPs buy a rate the design actually spends.
- **Give the ledger a DSP number to enforce.** Restated, unchanged, and now
  overdue: `resource_budget.dsp_percent` and `resource_actual.dsp` exist in the
  schema and in `tools/ledger/src/types.ts`, nothing writes them and no validator
  reads them. Two blocks have now been fixed by hand while the gate that should
  have caught the problem still does not exist. The ALM side has V5; DSP has
  nothing.
- **Run the full fast lane after a FULL build, as the last action before
  reporting.** `cmake -S . -B build && ninja -C build` with no target argument,
  then `ctest -L fast`, after the final commit. Anything else reports on
  binaries rather than on the tree. This is now the single most important line
  in this file.
- **Before changing a block, list every target that elaborates it.**
  `grep -B12 "TOP_MODULE <module>" tests/CMakeLists.txt`. For this block the
  answer was four, one of which lives in another subsystem's test directory and
  guards a charter property the block-level lanes cannot see.
- **Any sweep must clean every CONSUMER of the mutated file, not every consumer
  it scores.** Now enforced by `check_consumers` in
  `tools/sweep_terrain_lod.sh`, which refuses to start if the build system knows
  about a consumer the sweep does not. **Checked, rather than assumed, for the
  other two sweeps:** `zhao_geom_lod` and `zhao_geom_cull` each have exactly
  **one** consumer today, so `tools/sweep_geom_lod.sh` and
  `tools/sweep_geom_cull.sh` do not leak. They have no guard against acquiring
  one, which is precisely how this happened here — TERRAIN.LOD landed in phase 6
  with three block-level lanes and gained the phase 8 composition later. Port
  `check_consumers` to both when either is next touched.
- **Write the mutation sweep before the restructuring, not after.** Doing it in
  that order here is what turned "the tests are green" into "the tests are green
  AND section 2 had a hole in it", and the hole was in the pre-existing block,
  not in the change.

---

## Files Created in This Directory

- `FINDINGS-dsp-terrain-lod.md` - this file

---

## Files Examined

- `fpga/rtl/terrain/zhao_terrain_lod.sv` - the block restructured and measured
- `design/contracts/TERRAIN.LOD.md` - Latency, Target throughput, Q formats,
  Memory ownership and Synthesis sections all re-stated from measurement
- `design/blocks.yml` - TERRAIN.LOD already declared `latency: variable` and
  `backpressure: ready_valid`; the change needed no ledger edit
- `reference/include/zref/zref_terrain_lod.hpp` - the oracle, read to confirm the
  arithmetic; **not modified**
- `tests/terrain/terrain_lod_directed.cpp` - grew the right-hand side's flip point
- `tests/terrain/lod_dev.hpp` - read and confirmed self-timed; **not modified**
- `tools/sweep_terrain_lod.sh`, `tools/sweep_terrain_lod_mutants.py`,
  `tools/sweep_terrain_lod_preflight.py` - the new sweep, grown 34 -> 40
- `tools/sweep_geom_lod.sh`, `tools/sweep_geom_lod_preflight.py` - the template
- `tools/quartus/run_block_fit.ps1` - the measurement
- `reports/synthesis/zhao_block_fit.json` - both rows
- `reports/REMAINING_BLOCKERS.md` - the 213/201 section, now 176
- `tests/measure/measure_governor_lod.cpp` - the composed Duo-fairness lane that
  went red; **the fourth consumer of this module, and the one I failed to look
  for**
- `tests/CMakeLists.txt` - the authority on who elaborates what; four targets
  verilate `zhao_terrain_lod`
- `runs/CLAUDE-RUNS/.../FINDINGS-dsp-sequencing.md` - the pilot this repeats
