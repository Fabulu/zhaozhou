# Task Log: RUN-20260824-0754 — TERRAIN.NORMALS rate sequencing

**Created:** 2026-08-24 07:54 UTC+02:00
**Status:** In Progress

---

## Objective

`zhao_terrain_normals` 18 DSPs → 3, by sequencing six spatial cross-product
multiplies onto one shared lane. Values bit-identical to the oracle.

---

## Progress Timeline

### 07:54 — run opened, and done directly rather than delegated

Four subagents in a row were killed by server-side API 529s before completing any
gate — two attempts at the vertex cache, one at this block, plus the projector
merge which died *after* archiving and so lost nothing. **Doing this one directly
rather than losing a fifth.**

### 07:56 — oracle checked FIRST (V17)

`zref::terrain::face_normal`, `reference/include/zref/zref_terrain_normals.hpp:54`.
Resolves. The contract calls it "a thin view onto an existing ratified law, not a
second implementation".

### 08:00 — read the RTL, and found the predicted duplicate

Clean two-stage pipeline: stage 1 computes six 33×33 products into three 67-bit
lanes, stage 2 applies three `rescale16` calls.

**`rescale16` was called SIX times for three values** — three for the outputs at
`s2_nx/ny/nz`, and three more *solely* to judge degeneracy. Each call is a 67-bit
add, an arithmetic shift and **two 67-bit comparisons**, so half of that logic
existed only to be compared against zero.

Same defect `SURFACE.STAMP` carried (a 66-bit rescale computed twice, the second
time only for a ledger bit) and that `zhao_field_alu` still carries. **Predicted
from the audit's "repeated calls to the same expensive function" detector, and it
was there.**

### 08:05 — the operands ARE differences, and 33 bits is correct

```
logic signed [32:0] e1x, ...;
e1x = $signed({bx_i[31], bx_i}) - $signed({ax_i[31], ax_i});
```

Sign-extend 32 to 33, then subtract — the correct safe width for a difference of
two signed 32-bit values. **Not gratuitous slack.**

Width is therefore *not* available here, and I had claimed on the docket that it
was. This block's own contract declares a domain-limit lane **"uniform over
±4096 world units, reaching the fx16 output rails"** (`TERRAIN.NORMALS.md:158`),
with `:113` stating the rails are reached by **legal input**. A world coordinate
in fx16 needs 29 bits and a difference of two needs 30, so nothing reaches the
27-bit 1-DSP band. Walked back at `42b6209`.

### 08:10 — the change

One shared 33×33 multiplier walked over six terms:

```
0: e1y*e2z +acc0    1: e1z*e2y -acc0
2: e1z*e2x +acc1    3: e1x*e2z -acc1
4: e1x*e2y +acc2    5: e1y*e2x -acc2
```

Edges are **latched at accept** so a changing input cannot disturb the walk.
`acc2`'s final subtract lands on the same edge that raises `s1_valid`, so `s1_n2`
takes the combinational value rather than the register — same value, one cycle
earlier, keeping the walk at six steps rather than seven.

`tri_ready_o` becomes `!m_busy && !s1_valid`, **deliberately more conservative
than the old `s1_free`**: the walk then cannot be interrupted and `s1_valid`
cannot be overwritten mid-sequence. II and latency both become 7, which the
contract's `variable` already admits.

### 08:15 — MEASURED

| | before | after |
| --- | ---: | ---: |
| **DSP blocks** | **18** | **3** |
| ALMs (estimated) | — | 768 |
| registers | — | 738 |
| II / latency | 1 / 2 | 7 / 7 |
| capacity | — | **238,095** normals/frame against a demand of 2,000 |

Map-only at committed `bfc7471`, one Quartus job, 26.4 s. The 3 is exactly the
band prediction: one 33-bit product is 3 DSP blocks.

**Verilator `-Wall` clean.** All four `terrain_normals` tests pass — directed,
random, random_nightly, lint — so the values are bit-identical.

**The build log shows seven compile steps for `Vzhao_terrain_normals`**, which
confirms the model was genuinely re-elaborated rather than reused. That check
exists because this project has twice scored a stale model and believed it.

One dead signal removed — `s1_free`, whose only consumer `tri_ready_o` had
replaced. Caught by `-Wall`, not by reading.

---

## Files Created

- this run directory

---

## Decisions Made

**Sequenced rather than narrowed.** Width is unavailable on this block and I had
claimed otherwise; rate was available, untouched by that error, and reaches the
same 3 DSPs.

**Conservative ready rather than an overlapped walk.** At a demand of 2,000/frame
against a capacity of 238,095, overlapping would buy nothing and could only
introduce a hazard.

---

## Next Steps

- [ ] full `ctest -L fast` — expect exactly one pre-existing failure,
      `ledger_check` V16 `FIELD.SEQ.CORE`, recorded before starting
- [ ] mutation sweep in a worktree, every survivor adjudicated
- [ ] `design/budgets/workloads.yml`: `measuredII` null → 7
- [ ] archive

### 08:40 — THE CHAIN TEST CAUGHT A LOST NORMAL, and the block's own tests could not

Full `ctest -L fast`: **two failures**, one of them mine.
`terrain_tess_normals` — the TESS -> NORMALS chain — reported:

    FAIL: NORMALS answers exactly once per triangle: expected 0x80, got 0x7F

**127 normals where 128 were due. One was lost.**

**Cause: `idle_o`.** It read `!s1_valid && !s2_valid`, which *was* the whole
story before sequencing — nothing sat between the handshake and the result. Now
a triangle can be **mid-walk with both valids low**, so the block advertised idle
while still holding work, and a consumer draining on `idle_o` stopped one
triangle early. Fixed to `!m_busy && !s1_valid && !s2_valid`.

**Two things about this are worth keeping.**

**It presented in a way that was nearly misread.** Most of the failure lines
said:

    FAIL: the composed normal is the oracle normal: expected 0xFFFC0000, got 0xFFFC0000

**Identical.** The values were never wrong — the arithmetic was bit-perfect
throughout. Only the *count* was wrong, and every value comparison after the
desync failed against a shifted stream. A harness that prints **the value it
compared** rather than only a verdict is what made that legible in one read;
had it printed "FAIL" alone I would have gone looking in the multiplier.

**And the block's own four tests passed the entire time**, before and after,
because they are latency-agnostic — they wait for `valid`. **The integration test
was the only thing in the repository that could see this.** That is the argument
for owning one, and it is the second time in two days that a block-local suite
was green while a seam was broken.

All five now pass: directed, random, random_nightly, lint, and the chain.

### 09:20 — SWEEP: 12 attempted, 12 accounted, 12 caught, 0 discarded

The block had **no sweep at all**; one was written, modelled on
`sweep_terrain_lod.sh`. Its mutant table is a **Python module rather than a bash
array** for a specific reason that applies here too: this RTL forms its
differences with `$signed(...)`, and inside a double-quoted bash word `$signed`
expands to nothing — an anchor would then either never match or, worse, match
*different text than the one written down*, and the sweep would score a mutation
nobody authored.

The mechanical rename also produced a target that does not exist and kept one
belonging to the LOD block. Both corrected: a sweep that cleans the wrong
directories leaves mutant-derived model sources in the ones it never scored.

**Sweep 1 — 10 of 12, and NEITHER survivor was equivalent.** Both were real
holes, and both existed for the same reason: **the tests were written against a
block that had no state between accept and result.**

* **M09**, step 0 reading the *live* edge instead of its latched copy, survived
  because `drive()` dropped `tri_valid_i` but left all nine coordinate inputs
  unchanged for the whole drain — so live and latched were always identical. In
  ready/valid the producer is free to change its data the cycle after
  `valid && ready`, and this block now spends six more cycles walking over the
  latched edges. **Held stimulus was testing a protocol nobody promised.**
  Closed by poisoning all nine inputs plus `src_id` immediately after
  acceptance, which strengthens every existing case rather than only this one.
* **M10**, judging degeneracy on a pre-rescale lane, survived because nothing
  drove a cross product that is nonzero yet rounds to zero — **precisely the
  case the contract calls out as deliberate.** Raw fx16 unit edges give
  cross = (1,0,0), and `rescale16(1)` is `(1 + 32768) >> 16 = 0`, so every lane
  rounds to zero while the pre-rescale lane reads 1. Added as a directed case.

Neither could have been found by a value test. **The arithmetic was never
wrong** — both are holes that only open when a block's *timing* changes.

**Sweep 2 — 12 of 12.** Both closed, verified.

### 09:25 — AND I NEARLY REPORTED A FALSE FINDING

Between those two runs there was a third that I almost believed. It reported
M09 and M10 **still surviving**, and I was one step from concluding the fix had
failed.

**The worktree checkout had silently not taken.** It was still at `a18cda4`
rather than `9bf9b24`, so that run scored the *old* tests — it was sweep 1
repeated, not evidence about anything.

Two things made it catchable, and both were habits rather than luck:

1. **the pristine model hash was byte-identical between runs**
   (`74439b23dc7c0f26`). Expected here, since the RTL did not change — but it is
   what prompted a check rather than an acceptance;
2. **I checked the artifact, not the command.** The checkout *appeared* to
   succeed. `git rev-parse` said `a18cda4`, and grepping the worktree source for
   the poison string returned **0**.

That is the thirteenth instance in three days of *an artifact being real while
being an artifact of something other than what it was read as* — and the first
where the thing about to be fooled was me. **The rule holds in both directions:
a green result from a tool nobody watched run is not evidence, and neither is a
red one.**

### 09:35 — independent confirmation, from a tool that did not know what changed

Regenerating the AST scanner over all 92 modules:

    nonconstantMultiplyInstances   6 -> 1
    duplicateExpressions           1 -> 0

Neither number was told to it. The second is the duplicated `rescale16` the
audit's detector had predicted before the block was opened.

### 09:40 — gates

| gate | result |
| --- | --- |
| oracle resolves (V17) | `zref::terrain::face_normal` |
| differential bit-identical | all 5 lanes pass |
| mutation sweep | **12/12 caught, 0 discarded** |
| gaps closed | 2, both real, both verified caught |
| `ctest -L fast` | green apart from the recorded V16 baseline |
| ledger | **1 error, unchanged** — nothing introduced |
| DSPs | **18 -> 3**, map-measured at a committed commit |

`measuredII` recorded as 7. `requiredII` corrected from the contract's one-clock
placeholder to the demand-derived 833 — and noted as a field **no tool reads**,
in all seven entries, exactly as `verticesPerItem` was.
