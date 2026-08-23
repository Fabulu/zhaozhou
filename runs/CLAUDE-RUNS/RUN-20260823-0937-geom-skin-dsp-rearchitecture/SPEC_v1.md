# SPEC v1: GEOM.SKIN — size the multiplier farm to the frame demand

**Run ID:** RUN-20260823-0937
**Created:** 2026-08-23 09:37 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`zhao_geom_skin` fits at **72 DSPs on a 112-DSP device** — 64% of the chip for
one block, measured at `16df9ee`, `rtlCleanAtHead: true`. Bring it to the
**12–18 DSP** target without changing a single output bit against the shipped
oracle, and leave behind a **parameterised frontier** rather than one point.

Success:

- the required throughput **derived from the owner's frame demand**, not from a
  one-clock placeholder;
- a `MUL_LANES` parameter with **measured constrained fits at more than one
  setting**, so the frontier is data and not an argument;
- a differential against `zref::creature::skin_vertex` that is *bit-identical*
  on every case the old one covered **plus the operand extremes the old one
  never reached** — the gap `GEOM.SKIN.md` itself records as blocking this
  rewrite;
- a mutation sweep with forced regeneration and a binary-hash check, every
  surviving mutant either killed by a new check or named as equivalent;
- `ctest -L fast` green, ledger no worse than the baseline;
- nothing claimed as hardware-proven. This is **simulation, synthesis and fit**.

---

## Step 1 (ledger rule V17): the oracle resolves — CHECKED FIRST

Run before any RTL was written, 2026-08-23 09:50:

    npm run ledger:check
    => CHECK FAILED — 1 error(s) against 92 blocks / 40 ops
       V16: FIELD.SEQ.CORE ... formal recorded as "pending"

**That is the Field agent's gate, deliberately red, and not mine.** V17 is
green: `GEOM.SKIN` cites `zref::creature::skin_vertex`, which is defined at
`reference/src/zcreature/creature_core.cpp:222` and declared in
`reference/include/zref/zref_creature.hpp`, and the contract and the directed
test both name it. **The oracle resolves. Step 1 is clear.**

The V16 error is the run's ledger baseline. If a second error appears after my
commits, that one is mine.

---

## The rate derivation, which is the whole argument

`reports/REMAINING_BLOCKERS.md:1223` left this block explicitly **not** queued
for sequencing, with the right reason:

> Skinning is different. It transforms a vertex by a 3x4 matrix, and vertices
> are the highest-rate object in the geometry pipeline. … answering it needs a
> vertex-rate budget that nobody has stated.

**The owner has now stated it.** Sustained demand is **~120,000 skinned vertex
instances per 60 Hz frame** — the number at which the Measure degrades.

| quantity | value | source |
| --- | ---: | --- |
| `gpu_clk` period | 10.000 ns (100 MHz) | `fpga/quartus/shell_fit/zhao_shell_fit.sdc:4`, and the per-block SDC `run_block_fit.ps1` generates |
| clocks per 60 Hz frame | **1,666,666** | 100e6 / 60 |
| sustained demand | **120,000 vertices/frame** | owner ruling |
| **clocks available per vertex** | **13.88** | 1,666,666 / 120,000 |

Multiply work per vertex, from the law itself:

| path | signed 32×32 products |
| --- | ---: |
| two-weight | **18** (2 matrices × 3 rows × 3 terms) |
| rigid | **9** |

So the honest multiplier requirement is **18 products / 13.88 clocks = 1.30
multipliers**. The shipped block issues all eighteen in one clock.

**The block is over-provisioned by 13.9×.** The recurring cause named in the
brief — *parallel multipliers where the block's RATE does not require them* —
**is true here**, and it is true by an order of magnitude. Recorded plainly
because `REMAINING_BLOCKERS.md` guessed the other way, in good faith, without
the number.

### What the shipped 72 DSPs actually are

72 = **18 × 4**. A signed 32×32 in this block's combinational cone costs four
DSP blocks. The **six weight multiplies contributed approximately zero**: `w0`
and `w1` are 7-bit, and Quartus already put them in logic.

**This matters for the second design direction I was handed.** The
`(pb << 6) + w0*(pa - pb)` identity is real and I will use it — but it saves
**ALMs, not DSPs**, and the run must not claim otherwise. The DSP win is
entirely in the 32×32 farm.

---

## The architecture, and where it departs from the brief

### Lanes by TERM, not by row

The brief proposed **three row lanes**. I evaluated it and chose a different
axis, because the operand muxing is strictly cheaper:

- **Row lanes** (lane *r* owns output row *r*): each lane must mux its
  coordinate over {x, y, z} **and** its matrix element over six values, and a
  row's six products serialise into six cycles, so the earliest any row
  finishes is the last issue cycle.
- **Term lanes** (lane *t* owns coordinate *t*): lane 0 **always** multiplies
  by `x`, lane 1 by `y`, lane 2 by `z`. **The coordinate mux disappears
  entirely.** A whole row-product is then issued in ONE cycle as a 3-lane dot
  product, so `pa[r]` and `pb[r]` complete two cycles apart and the six
  row-products retire in six cycles instead of the accumulator walk needing
  eighteen.

Term lanes also make the accumulator trivial: with three lanes the row-product
is `translation + p0 + p1 + p2` in a single adder tree, not a three-cycle
read-modify-write.

### The parameter and its legal settings

`MUL_LANES` decomposes into `TL` term lanes × `RL` row-product lanes:

| `MUL_LANES` | TL | RL | issue slots (blend / rigid) | latency (blend / rigid) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 1 | 18 / 9 | **21 / 12** |
| 3 | 3 | 1 | 6 / 3 | **9 / 6** |
| 6 | 3 | 2 | 3 / 2 | **6 / 5** |

`MUL_LANES = 2` is **not legal and this is not an oversight**: 2 divides
neither 3 terms nor 3 rows, so a cycle would straddle two row-products and each
accumulator would need a masked multi-source adder. The elaboration refuses it.

### The frontier this predicts — to be replaced by measurement

Sustained vertices per frame = 1,666,666 / latency, all-blend (worst case):

| `MUL_LANES` | vertices/frame | vs. 120,000 demand |
| ---: | ---: | --- |
| 1 | 79,365 | **✗ FAILS — 66% of demand** |
| 3 | 185,185 | ✓ 1.54× |
| 6 | 277,778 | ✓ 2.31× |
| *shipped* | *1,666,666* | *13.9×, at 72 DSPs* |

**`MUL_LANES = 1` is a real point on the frontier precisely because it fails.**
A frontier with no failing end does not show where the wall is. `MUL_LANES = 3`
is the intended configuration.

---

## Widths, PROVEN before synthesis (gotcha §5)

`reports/QUARTUS_GOTCHAS.md` §5: 72-bit operands bought a 72×72 multiplier
where 32×32 was honest, at 28 DSPs instead of 18. The shipped skinner carries
67- and 75-bit lanes. Proving them down:

| quantity | bound | signed bits | shipped |
| --- | --- | ---: | ---: |
| `m*x` | ≤ 2^62 (both operands −2^31) | 64 | — |
| `pa` = 3 products + (m3 << 16) | ≤ 3·2^62 + 2^47 = 1.3835e19 < 2^64 | **65** | 67 |
| `pa − pb` | ≤ 2.767e19 < 2^65 | **66** | — |
| `w0·(pa − pb)`, `w0 ≤ 63` | ≤ 1.743e21 < 2^71 | **72** | — |
| `(pb << 6) + w0·(pa − pb)` | ≤ 2.629e21 < 2^72 | **73** | 75 |

Two bits come off the accumulator and two off the blend lane. These are adders,
not multipliers, so the saving is ALMs — but §5's rule is *prove the width,
then synthesise*, and an unproven width is exactly what §5 punished.

**The multiplier operands stay a full signed 32×32.** The audit's suggestion
that a bone matrix's 3×3 is a bounded rotation is true of the *content* and not
of the *contract*: the oracle accepts any s32, and narrowing would be a
behavioural change disguised as an optimisation.

---

## The two things `GEOM.SKIN.md` says must be closed BEFORE the rewrite

Both are in the contract's own words, and both are load-bearing here.

**1. The identity is FALSE for `w0 > 64`.** `w1 = 7'd64 - v_w0_i` wraps, so the
shipped form computes `192 - w0` where the identity reads `64 - w0` as
negative: 223,020 mismatches in the contract's sampled sweep. It is benign only
because `w0 > 64` is out of contract. **The rewrite needs an `ENFORCED-BY:`
naming who upholds `w0 <= 64`, not a comment asserting it.**

**2. The differential does not reach the operand extremes.** The random lane
shifts matrices down 15 bits and vertices 8, so `pa` reaches ~2^43 against a
lane that holds ±2^66. My narrowing to 65/73 bits is *proven* above — but the
proof lives in a paragraph, and a paragraph is not a check. **This gap is
closed before the RTL is trusted, not after**: directed row-product rail cases
plus a full-range sub-lane, so `pa`, `pb` and `pa − pb` are driven to their
actual bounds.

---

## Scope

**In Scope:**

- Rearchitect `fpga/rtl/geometry/zhao_geom_skin.sv` around a `MUL_LANES`
  multiplier farm local to this block.
- Extend `tests/geometry/geom_skin_directed.cpp` with the operand extremes, and
  adapt its driver to the multi-cycle handshake without losing a single
  existing check.
- Mutation sweep, forced regeneration, binary-hash check, seven guards, lint
  preflight.
- Constrained per-block fits at more than one `MUL_LANES` setting.
- Update `design/contracts/GEOM.SKIN.md` and `design/blocks.yml` (latency and
  target_throughput are both wrong the moment the RTL changes).

**Out of Scope:**

- GEOM.CULL, SURFACE.STAMP, TMU, NORMALS, TERRAIN.PROJECT — queued behind this
  and handled serially.
- Any console-global multiplier farm. Explicitly rejected: share **within** a
  subsystem only, smallest local farm, sharing only what is mutually exclusive.
- Particle-simulation, compositor and 2D behaviour — owner docket, not mine.
- Any claim about physical hardware.

---

## Constraints

- **No Quartus fit until the machine is free.** A constrained `zhao_field_seq`
  fit was running at run start; three concurrent fits exhaust this 24 GB
  machine. Design, RTL, differential and mutation work come first.
- **No build until the coordinator confirms the clean rebuild is green.**
  `build/` was wiped and is being reconfigured; two builds in one build dir
  corrupt each other.
- Build via presets only, and the env script must be dot-sourced in the **same**
  invocation:
  `. .\tools\env\zhao-env.ps1; cmake --preset windows-native; cmake --build build`
- Ledger baseline is **one** error (V16 FIELD.SEQ.CORE). A second is mine.
- Verify every fit was constrained: `Info (332111):   10.000          clk`.
- Never hand-edit `reports/synthesis/zhao_block_fit.json`.
- Commit and push logical commits during the run, not batched at the end.

---

## Don't Retry

*Failed approaches, so they are not re-learned after compaction.*

- **Do not trust `(* multstyle = "logic" *)`.** Quartus 17.0.2 accepts it and
  does nothing. The only symptom is a DSP count that will not fall. Write a
  narrow multiply as a shift-add instead.
- **Do not assume the weight multiplies are DSPs.** They are not; 72 = 18 × 4 is
  the 32×32 farm alone. The blend identity is an ALM saving.
- **Do not hash `V<top>.cpp` to decide whether a model re-elaborated** — it is
  byte-identical between pristine and mutant. Hash the whole model directory.
- **Do not stamp a mutant's mtime into the future** to force a rebuild; the
  mutant model then outranks the restored pristine source.
- **Do not score a mutant that failed to COMPILE as caught.** The executable
  lives outside the target directory, so a failed build silently re-runs the
  previous binary. That error inflated a real 22/23 to a reported 21/22.
- **Do not run `cmake --preset windows-native` without dot-sourcing
  `tools/env/zhao-env.ps1` first.** The devkitPro msys2 cmake shadows the
  Windows one and reports every preset as "disabled", which is a lie about the
  preset and the truth about the PATH.

---

## Open Questions

- Does a registered 32×32 lane cost 3 DSPs or 4 on this device? The shipped
  combinational form measured 4. `zhao_field_mul`'s registered 33×33 measured
  3 for the whole Field engine. Only the fit answers it, and the answer decides
  whether `MUL_LANES = 3` lands at 9 or 12.
- Who actually enforces `w0 <= 64`? GEOM.VDECODE is the upstream in the ledger;
  the `ENFORCED-BY:` must name a real check, not an intention. If no check
  exists, that is an owner-docket item, not something to paper over.
