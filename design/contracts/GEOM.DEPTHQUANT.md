# Contract — GEOM.DEPTHQUANT (Depth profile application)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: the depth-profile table is generated; the applying function is
> PLANNED AND NOT WRITTEN

## Purpose and exclusions

GEOM.DEPTHQUANT converts the projector's reciprocal into the depth every
consumer already believes it is receiving, under the selected profile.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R6, and verified
independently against the tree:**

* `zhao_geom_project.sv` emits **`out_d_o`, documented "Q16.16 1/w"**;
* **twelve RTL files consume `invw24`** — `raster_attrdiv_svc`,
  `geom_attrsetup`, `geom_parambuf`, `raster_perspuv_svc`, `raster_earlyz`,
  `raster_fragment`, `geom_clip`, `raster_texjoin`, `raster_perspuv`,
  `raster_rcp24`, `raster_attrinterp`, `texture_tmu`;
* **no `depth_profile` port exists anywhere in `fpga/rtl/`**, though the ABI
  carries a two-bit profile in `SetView`.

**This is not a capacity concern. It is a correctness one.** Twelve consumers
use a value called `invw24` whose producer emits something else, and the
conversion currently has no home — which means it will otherwise appear as
integration glue, differently in each place that needs it.

`DEPTH_PROFILE_NEXT_STEPS` steps 5–6 are open, and step 5 is described there as
**"the only thing that was ever actually blocked"**. The docket lists steps 1–4
DONE and is silent on 5–6, so it reads closed. It is not.

**Exclusions, each a specific refusal:**

* **No projection.** `GEOM.PROJECT` owns the transform and the reciprocal.
* **No depth test.** `RASTER.EARLYZ` owns comparison; this block owns format.
* **No clipping decision.** Behind-eye handling is `GEOM.CLIP`'s; this block
  only says what depth value a behind vertex carries.
* **No new profiles.** The profiles are ruled and generated; this applies them.

## Input and output packet layouts

**In:** `{ d_q16 (signed 32, Q16.16 1/w), behind (1), profile[1:0], src_id }`.
**Out:** `{ invw24 (u24), saturated (1), src_id }`.

`profile[1:0]` comes from `SetView`'s ratified field. **It is carried per
vertex rather than latched as global state**, because a capture must be
replayable without depending on command ordering — and because two views may
legally differ.

## The one job, stated so it cannot be reinvented

    invw24 = saturate_u24( round( d_q16 * scale(profile) >> shift(profile) ) )

with `scale` and `shift` taken from the **generated** profile table, not
restated here. Restating them is how a table and its user drift, which is the
`QFMT_VERSION` failure in a different costume.

## Backpressure rules

Ready/valid, one vertex per clock. It is a multiply, a shift, a rounding and a
saturation; it must not be the thing that limits the geometry path.

## Memory ownership

None. The profile table is small enough to be constants selected by
`profile[1:0]`.

## Q formats and rounding

* in: **Q16.16**, signed, as `GEOM.PROJECT` emits it
* out: **u24**, unsigned, larger is closer — the convention the twelve
  consumers already assume
* **one rounding**, round-half-up per `spec/qformats.md` §3

## Latency (fixed or variable)

`fixed`, small.

## Overflow and malformed-input behaviour

* **Saturation is a REPORTED event, not a silent clamp.** A depth that
  saturates is geometry outside the profile's range; the count is how anyone
  discovers a profile is wrong for the content.
* **A behind-eye vertex** (`clip.w <= 0`) does not get a converted depth. The
  near-plane law drops the whole triangle, so this block emits the profile's
  declared behind value and flags it rather than producing a plausible number
  from a meaningless reciprocal.
* **A negative reciprocal is malformed** — `1/w` for a vertex in front of the
  eye is positive — and is refused and counted rather than wrapped into a large
  unsigned depth, which would read as "very close" and win every depth test.

## Scalar reference function

**PLANNED AND NOT WRITTEN**: `zref::geom::depth_quant(d_q16, profile)`, which
must be **generated from or checked against the same profile table the RTL
uses**, never a second copy of the numbers.

## Directed tests

**PLANNED AND NOT WRITTEN**:

* each profile's exact `wmin`/`wmax` boundary, both sides;
* saturation reported rather than clamped silently;
* a behind-eye vertex flagged, not converted;
* a negative reciprocal refused rather than wrapped;
* **the profile travels with the vertex** — two views with different profiles
  in one frame produce different depths for the same world position.

## Randomized differential tests

Planned, against the scalar function across the full Q16.16 input range with a
deliberate out-of-range fraction, and a coverage guard that each profile and
each refusal class was reached.

## Integration capture cases

None on hardware. **The composed case that matters**: `GEOM.PROJECT` →
DEPTHQUANT → `RASTER.EARLYZ`, because that is precisely the seam where a value
called `invw24` currently comes from a producer that emits Q16.16.

## Synthesis / resource ceiling

Small: one multiplier, a shift, a rounding adder, a saturate. Likely 1 DSP or
none, depending on the scale widths.

## Notes

**RULED, D-4, 2026-09-03: a separate named block.**

> It may physically sit immediately beside — or eventually be instantiated
> inside — the project wrapper. **It remains a separate ledger row, contract,
> oracle and test target so unfinished depth work cannot hide inside an
> otherwise green `GEOM.PROJECT`.**

And the consumer rule that comes with it, which is the reason the block exists
at all:

> **All downstream consumers receive only the canonical `invw24`. No consumer
> performs its own profile conversion.**

The conversion happens **once per projected vertex, before the value enters
clipping, parameter storage and rasterisation** — not at twelve call sites that
would drift.
