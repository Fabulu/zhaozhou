# Contract — GEOM.SKIN.NORM (Blended world normal)

> Ledger: `design/blocks.yml` · gpu clock · maturity **UNIT_VERIFIED**
> RTL: `fpga/rtl/geometry/zhao_geom_skin_norm.sv` — **built 2026-09-04**
> Reference: **`zref::creature::skin_world_normal`**
> (`reference/src/zcreature/creature_core.cpp`)

## Purpose and exclusions

Transform a packed bind-space normal through the same two-bone blend as its
vertex, blend the **vectors**, renormalise **once**, and hand out the pair
`{direction, magnitude}`.

**Why it exists as a block at all.** `GEOM.LIGHT`'s contract named `SKIN.NORM`
as a prerequisite that is not itself, and on 2026-09-04
`tools/design/compose_order.py` reported the consequence in one line:
*`GEOM.LIGHT takes world_normal — no upstream emits it`*, the last unexplained
input in the geometry subsystem. Every other orphan that day turned out to be a
naming collision. This one was a missing block.

**Why it is not part of `GEOM.SKIN`.** `reports/CREATURESANDLIGHTS` states that
block fits at 89.65 MHz with 9 DSPs and one weighted vertex per twelve clocks,
and that **"nothing more may be bolted onto its output"**. This law needs the
normal transformed by *both* bones — 18 multiplies — plus a square root.

**Exclusions, each a specific refusal:**

* **No Lambert.** The dot against a light direction is `GEOM.LIGHT`'s, and the
  split between them is the whole point of this block.
* **No square root.** `zhao_field_isqrt` already cites `zref::isqrt_u64` and
  returns the true floor; a second implementation would be a second law.
* **No palette.** `GEOM.POSE` owns the bone cache. The caller presents two
  resolved matrices; a lookup here would be a second cache.
* **No normalising to unit length.** See below — it would cost a rounding the
  law does not have.

## The law

`spec/creature_rules.md` §2.x.1, **repaired 2026-09-04** after the spec was
found carrying a ratified law its own oracle never implemented:

    n[row] = w0 * (A_row . N) + w1 * (B_row . N)    -- blend the VECTOR
    range-reduce until max|n| < 2^30                -- direction preserved
    mag    = isqrt_u64(n.n)                         -- ONE renormalisation

The struck law blended each bone's already-clamped Lambert response, which makes
light follow influence weights instead of the deformed surface and produced
visible bright patches at mixed-weight joints.

## ONCE PER VERTEX, NOT ONCE PER LIGHT

Owner, `reports/CREATURESANDLIGHTS`: *"The current reference repeatedly calls
`skin_normal_lambert` for key, fill and point light … The hardware should not
reproduce that structure."*

Everything here is light-independent, so it runs once and each light afterwards
costs `GEOM.LIGHT` one dot and one divide.

**That equivalence is proved, not assumed.**
`tests/geometry/skin_norm_split_directed.cpp` — one normal reused across three
lights against three independent `skin_normal_lambert` calls, 20,000 vertices,
with a majority required to be lit because two structures agreeing on darkness
is not agreement. It was written **before** this block existed, so the structure
was licensed before it was built.

## Q formats and rounding

| value | format | rounding |
|---|---|---|
| packed normal | s8 x3 | none — carried |
| blended direction | s64, fx16-scaled by the bone matrices | **none** |
| magnitude | u64 | `isqrt_u64`, exact floor |

**The output is `{direction, magnitude}` and NOT a unit vector.** Normalising
here would round twice — once into the unit vector and again in the Lambert
quotient — and the law has exactly ONE rounding. The pair is the interface for
that reason and not for convenience.

**The accumulator is 64 bits because the oracle's is.** If the reference can
overflow on a hostile input, the RTL must overflow identically, or the two part
company exactly where nobody looks.

## Degenerate vertices

Two ways a surface has no direction, and both are reported rather than lit:

* a **zero packed normal**, refused before the palette is touched;
* a **blend that cancels** to zero length.

Both raise `n_degenerate_o` with `mag = 0` and increment `degenerate_o`.
`GEOM.LIGHT` must leave such a vertex black rather than receive a zero-length
vector to divide by.

## Backpressure rules

Ready/valid on both sides, and one `zhao_field_isqrt` service turn per live
vertex. Sits behind `GEOM.SKIN`'s one-per-twelve-clocks, so the sequenced
transform costs nothing that matters.

## Scalar reference function

`zref::creature::skin_world_normal` (`reference/src/zcreature/creature_core.cpp`,
declared in `reference/include/zref/zref_creature.hpp`), with its partner
`zref::creature::lambert_from_world_normal` owned by `GEOM.LIGHT`.

**`skin_normal_lambert` is now literally their composition**, so the split this
block depends on is true by construction rather than by a second implementation
that agrees until it does not. The refactor that created the boundary was proved
behaviour-identical against the pre-refactor code lifted from git — 200,000
comparisons, zero mismatches.

## Directed tests

**`tests/geometry/skin_norm_rtl_directed.cpp` — WRITTEN**, 5 checks against
`zref::creature::skin_world_normal`: the pair compared exactly over 402
vertices, the degenerate case counted, and an identity-bone case whose answer
can be read without running anything.

The testbench **plays** `zhao_field_isqrt` rather than instantiating it: that
block has its own differential, and instantiating it here would re-prove the
square root while hiding whether this block handed it the right sum of squares.

**Mutation tested on the path that fires 10% of the time.** Disabling the range
reduction fails 38 comparisons with the RTL reporting exactly **2x** the
oracle's values — a missing single shift. The mutation targeted that branch
deliberately, because one the common path catches proves nothing about the rare
one.

## Notes

39 of 402 random vertices needed the range reduction, so the branch is exercised
by the ordinary sweep rather than only by a constructed case.
