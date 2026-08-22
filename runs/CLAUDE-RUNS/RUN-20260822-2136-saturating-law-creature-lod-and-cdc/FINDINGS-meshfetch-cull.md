# GEOM.MESHFETCH conservative frustum cull — Findings

**Agent ID:** claude-meshfetch-cull
**Created:** 2026-08-22
**Parent Task:** RUN-20260822-2136
**Status:** Complete

> Everything here is SIMULATION and synthesis-target design. No hardware has run
> any of it, and this block has never been through a Quartus fit.

---

## Summary

The owner's 2026-08-22 ruling — conservative per-camera frustum rejection of an
instance bounding sphere, reject only when outside EVERY active camera — is now
a reference (`zref::cull`), an RTL block (`zhao_geom_cull`), a differential that
pins the camera half to the SHIPPED renderer, and a mutation sweep. The meshlet
descriptor fetch is deliberately absent: its format is still unfrozen, and the
block takes the bound as ports so it is not blocked on that decision.

The load-bearing result is that the rounding direction is now *measured to be
observable*, not merely argued: 1,015 of 1,775 boundary probes give a different
verdict under a floor bound than under the ceiling, so the test can see the one
mistake that would delete geometry.

---

## Findings

### 1. The derivation reproduces, independently

`cull_check.cpp` was recompiled and re-run before anything was trusted:

```
plane equivalence : 600000 checked, 0 mismatches
conservatism      : 60000 spheres, 26796 rejected, 0 WRONGLY rejected
looseness         : 344 kept although empty
```

identical to the numbers in `MESHFETCH_CULL_DESIGN.md`. The same three claims
were then re-run through the committed reference API and reproduced (60,000
spheres, 0 wrongly rejected, 383 loose — the small drift in the loose count is
the PRNG stream moving, not the arithmetic).

### 2. The reference is BOTH kinds of reference at once, and they are kept apart

`reports/PHANTOM_REFERENCES.md` separates a reference that already exists under
another name (kind 1) from one that must be written and thereby *becomes* the
law (kind 2). `zref::cull` is both:

| half | kind | what pins it |
| --- | --- | --- |
| the five planes / the clip convention | 1 | `zref::render::project_vertex`, `mat4_vec4`, `fx_div_exact` — shipped |
| the sphere-vs-plane test and the ceiling bound | 2 | nothing shipped; the header IS the law |

The kind-1 half is checked in the committed differential, not in a scratch file:
360,000 random points across six cameras, compared against BOTH the exact
clip-space form of the shipped row product AND the end-to-end
`project_vertex` + `fx_div_exact` path. **0 mismatches on both, and 0 points
that the planes reject but the renderer would have drawn.**

The kind-2 half cannot be pinned that way, so it is defended two ways: by proof
(the bound is a ceiling, so `dot < -r*len_hi` implies `dot < -r*|n|`, which is
the true "wholly outside" condition) and by section 5 of the differential, which
*measures* that the test distinguishes the ceiling from the floor.

`zref::MeshFetch` stays a phantom. This is `zref::cull`, and the header says so.

### 3. A plane component does not fit in 32 bits, and the sum of squares does not fit in 64

A plane component is `row3[j] ± rowk[j]` — the sum of two fx16 words — so its
range is `[-2^32, 2^32-2]`. Carried as `int64` in the reference and signed 33 in
the RTL. Consequently `a^2+b^2+c^2` reaches `3*2^64`, which is **outside u64**,
so `zref::isqrt_u64` cannot be called directly.

The fix is a widening, not a second algorithm: `zref::cull::cull_isqrt` is the
identical §7.2 restoring recurrence with the starting bit moved from `4^31` to
`4^32`. It is checked against `isqrt_u64` exhaustively for n < 100,000 and over
200,000 random u64 draws, and above u64 against the defining property
`res^2 <= n < (res+1)^2`. The alternative — clamping the argument — would have
made every length bound depend on a saturation nobody had analysed.

### 4. The RTL is not a transcription of the reference, which is what makes the differential worth something

Four genuine differences, each a place a retype-check would prove nothing:

* The reference stores five planes per view. **The RTL stores none** — it
  rebuilds any plane from the registered matrix with four 33-bit adds behind a
  five-way mux. That saves ~1,320 flops and costs one new failure mode (a mux
  that selects the wrong row), which section 2/3 and the sheared cameras exist
  to catch.
* The reference calls a 128-bit `cull_isqrt`. The RTL runs the recurrence over
  33 clocks in 66-bit registers, starting unconditionally at `4^32` where the
  software has a `while (bit > num)` prologue.
* The reference takes the ceiling by squaring the root back. **The RTL reads the
  recurrence's own remainder**, which is `n - res^2` — a different computation
  that happens to be equal. Checked directly.
* The reference is a pure function; the RTL is a sequencer whose dirty bit is
  the only thing preventing a cull against a *previous* camera's length bounds.

### 5. A symmetric camera cannot see a swapped plane pair — so the test set is sheared

On a symmetric perspective camera, left and right are mirror images and so are
top and bottom. Swapping either pair produces the **identical set of five
planes**, so the frustum is unchanged and no input can distinguish the mistake.
Three of the six cameras in the differential therefore have an off-centre
principal point (`shear_x` / `shear_y` push `row0[2]` and `row1[2]` off zero),
which breaks the mirror.

This also predicts, before the sweep runs, that a *label* permutation of the
planes is an equivalent mutant — see the sweep section.

### 6. Where the work lives

The five planes and their five length ceilings are per camera per frame; the
sphere test is per instance and there may be thousands. So the five square roots
run on the **config** path (185 cycles per view, on a matrix write) and never on
the instance path (10 cycles, one plane per cycle, three 33x32 multipliers plus
one 32x34). The same move `zhao_geom_lod` made when it turned its divides into
comparisons.

**No Quartus number exists for this block.** Every width in it is argued from
its range, not measured. `zhao_geom_lod`'s history (72-bit slack cost 28 DSPs
against 18 for the honest width) is the reason the multiplier operands are
declared at exactly the widths their ranges need.

---

## The mutation sweep, and the one prediction it falsified

`tools/sweep_geom_cull.sh` (with `tools/sweep_geom_cull_preflight.py`), carrying
all five phantom-run guards from the LOD sweep unchanged:

```
linted 32 mutants, 0 do not build
attempted=32 expected=32 accounted=32 caught=30
SURVIVOR: M08 top and bottom swapped (EXPECTED EQUIVALENT)
SURVIVOR: M29 clear dominates set on the dirty bit
```

**The preflight earned its place on the first run.** M06 originally deleted the
`d` term from the plane dot product, which leaves `pl_d` unused and fails
`-Wall`. Under the old scheme that mutant would have been *discarded*; before
guard 5 existed it would have been scored *caught*. The preflight refused to
start instead, and M06 was restated as a sign flip on the same term — which is
caught.

### M08 is equivalent, as predicted

The five planes are used only as a SET (outside a camera iff outside at least
one), so relabelling two of them cannot change the answer:
`{row3+row1, row3-row1}` is the same pair of half-spaces however they are named.
The one thing that could still break it — the pairing of a plane with its stored
length bound — cannot come apart, because extraction and evaluation reach the
plane through the SAME `sel_plane` mux and index `len_ceil` with the same
number.

A plane built from the WRONG ROW is not a relabelling and dies: M02, M04 and M05
are all caught, and the sheared cameras are why.

### M29 was predicted CAUGHT, survived, and is the more useful of the two

I wrote in the RTL that if the dirty-bit clear won over the set, "the write would
be swallowed and the block would go on culling against a length bound belonging
to the previous matrix". **The sweep disproved that.** The clear fires only when
`prep_start` does — state S_IDLE, extraction about to begin — and a matrix write
in that cycle commits on the same edge, while the extraction just started makes
its first read of `mat` in the NEXT cycle through a combinational mux. So the
write whose dirty bit was discarded is already in the matrix being extracted.
Every later write lands while the state is not S_IDLE, where nothing is cleared.

No input distinguishes the two orderings. The comment was rewritten to say what
is actually true: the ordering is **defensive, not load-bearing**, and it matters
only if the extraction ever latches `mat` at `prep_start` instead of reading it
combinationally — which is exactly the change a timing pass would make. Both
proofs are recorded in the sweep script's header, not as bare labels.

This is the second time in this run that a claim of the form "correct by
construction" turned out to be true for a different reason than the one written
down.

---

## Gates

| gate | result |
| --- | --- |
| `ctest -L fast` | **262 / 262 passed**, including `geom_cull_directed`, `geom_cull_random`, `lint_zhao_geom_cull` |
| `npm run ledger:check` | **OK** — 92 blocks / 40 ops, V1–V17 + V19–V22 green |
| `verilator --lint-only -Wall` | clean |
| pinned `clang-format` (LLVM 15) | clean |
| directed differential | 11,090 instance verdicts, 17,212 checks |
| mutation sweep | 32 attempted / 32 accounted / 30 caught / 2 equivalent (both proved) |

`GEOM.MESHFETCH` stays **SPECIFIED**. Two of its three jobs now have RTL, but
`zref::MeshFetch` still resolves to nothing and the meshlet schema is unfrozen;
pointing the ledger's `reference_model` at `zref::cull` would be exactly the
mistake `reports/PHANTOM_REFERENCES.md` exists to prevent.

---

## Recommendations

- Fit the block. `zhao_geom_lod` is the standing proof that width arguments and
  DSP counts are not the same thing, and the instance path here has four
  multipliers whose sequencing lever is named in the RTL header but not costed.
- Leave `GEOM.MESHFETCH` at `SPECIFIED`. Two of its three jobs now have RTL, but
  the descriptor fetch has no format and `zref::MeshFetch` still resolves to
  nothing.

---

## Files Created in This Directory

- `FINDINGS-meshfetch-cull.md` — this file
- (`MESHFETCH_CULL_DESIGN.md` and `cull_check.cpp` were inherited, not created)

## Files Created / Changed in the Repository

- `reference/include/zref/zref_cull.hpp` — the reference (new)
- `fpga/rtl/geometry/zhao_geom_cull.sv` — the RTL (new)
- `tests/differential/geom_cull_directed.cpp` — the differential (new)
- `tools/sweep_geom_cull.sh`, `tools/sweep_geom_cull_preflight.py` — the sweep (new)
- `tests/CMakeLists.txt` — registration + the lint lane

## Files Examined

- `runs/.../MESHFETCH_CULL_DESIGN.md`, `cull_check.cpp` — the derivation, re-run
- `fpga/rtl/geometry/zhao_geom_lod.sv` — the block-boundary pattern
- `fpga/rtl/geometry/zhao_geom_project.sv` — the config port and `mul32` width idiom
- `reference/src/zrender/rast.cpp`, `zrender/internal.hpp` — `project_vertex`
- `reference/include/zref/zref_fixp.hpp`, `zref_trig.hpp` — `mat4fx`, `isqrt_u64`
- `tools/sweep_geom_lod.sh` + preflight — the five phantom-run guards
- `docs/OWNER_DOCKET.md` — the ruling
