# DSP sequencing on zhao_terrain_lod - Findings

**Agent ID:** claude-dsp-terrain-lod
**Created:** 2026-08-23
**Parent Task:** RUN-20260822-2136
**Status:** Complete

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
- `runs/CLAUDE-RUNS/.../FINDINGS-dsp-sequencing.md` - the pilot this repeats
