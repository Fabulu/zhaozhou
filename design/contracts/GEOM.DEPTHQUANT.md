# Contract — GEOM.DEPTHQUANT (Depth profile application)

> Ledger: `design/blocks.yml` · gpu clock · maturity **UNIT_VERIFIED**
> RTL: `fpga/rtl/geometry/zhao_geom_depthquant.sv` — **built 2026-09-04**
> Reference: **`zref::depth_of_raw`** (`reference/include/zref/zref_depth.hpp`)
> — it already exists and is complete, with the generated profile table in
> `reference/include/zref/generated/zref_depth.hpp`

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

## CORRECTION, 2026-09-03: THE INPUT IS `w`, NOT `1/w`

The first draft of this contract said the input was the projector's Q16.16
`1/w` and the job was a multiply and a shift. **Both were wrong, and reading
`reference/include/zref/zref_depth.hpp` is what corrected them.**

The ratified law (`spec/qformats.md` §8, owner ruling 2026-08-31 #1) is:

    s      = smallest shift with (W >> s) < 2^24     -- W is w in fx16 raw
    {r, k} = rcp_u24(W >> s)
    d      = rescale(SCALE * r, 48 + s - k), round-half-up, saturate 0xFFFFFF

**It consumes `w` and performs its OWN reciprocal.** It is not a rescale of a
reciprocal somebody else computed.

### And the projector does not expose `w`

* `zhao_geom_project.sv` outputs `out_d_o`, documented **"Q16.16 1/w"**;
* `zhao_project_core.sv`'s output list is `out_valid_o`, `out_d_o`,
  `out_behind_o`, `out_view_o`, `out_payload_o` — **no `w`**;
* yet the core *has* it: *"The three quotients share the divisor `clip.w`"*.

**So `w` exists one wire away from where it is needed and is discarded.**

### The resolution, and it is cheap

**Expose `clip.w` from `zhao_project_core` as an additional output and feed
this block with it.** It is a wire, not arithmetic — the core already holds the
value because it divides by it. The alternatives are worse: reciprocating
`1/w` back to `w` loses precision to answer a question the projector could
have answered exactly, and re-expressing the ruled law in terms of `1/w` means
a second depth law, which is the defect class this repository found twice
today already.

**This is a required change to `GEOM.PROJECT`/`zhao_project_core`**, named here
rather than assumed, and it is the kind of thing worth discovering while
writing a contract instead of while writing RTL.

## Input and output packet layouts

**In:** `{ w_fx16 (u32 or wider, fx16 raw), behind (1), profile[1:0], src_id }`.
**Out:** `{ invw24 (u24), saturated (1), src_id }`.

`profile[1:0]` comes from `SetView`'s ratified field. **It is carried per
vertex rather than latched as global state**, because a capture must be
replayable without depending on command ordering — and because two views may
legally differ.

## The one job, stated so it cannot be reinvented

The law above, verbatim from `zref::depth_of_raw`, with **every constant taken
from the generated table** (`gen::DEPTH_PROFILES`) rather than restated.
Restating them is how a table and its user drift — the `QFMT_VERSION` failure
in a different costume, and that one actually happened this morning.

Three details the oracle makes explicit and the RTL must copy:

* **the clamp to `[wmin, wmax]` is part of the LAW, not a caller courtesy.**
  `wmax` is a depth **clamp**, not a far-clip plane, so a `w` beyond it is
  legal geometry sharing the floor depth rather than being culled;
* **the product reaches ~2^80**, so the intermediate is 128-bit. A 64-bit
  accumulator *"would truncate silently and produce a plausible wrong depth"*;
* **an out-of-range shift returns 0 rather than a wrong number.** The three
  shipped profiles never produce it; a fourth that did would be unusable, and
  *"returning a wrong number quietly is worse than clamping loudly at the
  floor."*

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

`zref::depth_of_raw` — **ALREADY WRITTEN AND COMPLETE**
(`reference/include/zref/zref_depth.hpp`), over the generated table in
`reference/include/zref/generated/zref_depth.hpp`.

**So this block needs no new oracle at all**, which is the happiest form this
section can take. The RTL is checked against the existing law, and "RTL ==
oracle" already means "RTL == what `spec/qformats.md` §8 ratified".

## Directed tests

**THE LAW IS ALREADY TESTED**, and this block inherits those proofs rather than
restating them:

* `tests/proofs/depth_oracle_directed.cpp` — the oracle against the law;
* `tests/proofs/depth_profile_law.cpp` — the profile law itself;
* `tests/proofs/depth_profile_abi_directed.cpp` — the ABI carrying the profile.

**What is NOT yet tested is RTL**, because there is none. The cases that will
matter when there is, none of which the proofs above can cover:

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

**Larger than the first draft assumed**, because the job is a reciprocal and a
128-bit-intermediate multiply, not a rescale. It needs an `rcp_u24` — and the
console already has one: `zhao_field_rcp24_rom` backs both
`zhao_raster_rcp24_svc` and `zhao_raster_rcp24.sv`. **Reuse it.** A second
reciprocal ROM would be a second law.

The `SCALE * r` product is ~2^80, so the multiply is wide. Whether that is one
DSP cascade or several is a fit question; what is not negotiable is that the
intermediate cannot be 64-bit.

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

## What building it found

Three faults, all of which lint had passed, all found by the directed test
rather than by reading the code back:

* **The normalisation loop descended.** `s` is set by the HIGHEST set bit, and
  an unrolled priority chain gives the LAST assignment. Descending picked the
  *lowest* set bit above 23, so a `w` with a run of high bits under-shifted by
  the width of that run. Depth at `wmax` came out exactly 2^5 too large — a
  number that is plausible, monotonic, and wrong.
* **A shift of zero was refused as out of range, and zero is the near pin.**
  The generator SOLVES each profile's `SCALE` so the shift lands on exactly
  zero at `wmin` and the depth *is* the raw reciprocal — which is how `0xFFFFFF`
  is hit exactly rather than one short. Refusing it returned depth 0, the value
  meaning "far", for the closest geometry in the scene. This holds for all three
  profiles and is a property of the solve, not a coincidence.
* **The reciprocal handshake never consulted `rcp_ready_i`.** Valid was asserted
  and the response awaited without the request having been taken. Against the
  free-running service in the testbench this works; against a busy one it
  deadlocks. Split into `S_RCP` (issue, wait for accept) and `S_WAIT` (wait for
  answer).

The first two are the same class of fault: a value that is wrong by a clean
power of two, in the direction that still sorts correctly. Nothing downstream
could have detected either.

## Implementation notes

**It calls the existing `rcp_u24` service.** A second reciprocal ROM would be a
second law, and the two would agree until they did not.

**`SCALE` is a power of two for all three shipped profiles**, so `SCALE * r`
collapses to a shift of `r` and the oracle's 128-bit intermediate is never
materialised. The shift AMOUNT is still computed in a signed 9-bit width that
cannot wrap. **The test asserts the power-of-two property** rather than assuming
it, so a fourth profile whose `SCALE` is not a power of two fails loudly instead
of shifting wrongly — the RTL says so in a comment and the test proves it.

## Required change to GEOM.PROJECT

`zhao_project_core` **has** `w` — "the three quotients share the divisor
`clip.w`" — and does not expose it. **Until that port exists this block has no
producer in the shell.** That is the one outstanding integration item.

## Directed tests

**`tests/geometry/geom_depthquant_directed.cpp` — WRITTEN**, 7 checks, against
`zref::depth_of_raw` (the ratified law the golden captures already pin) rather
than a second implementation of it:

* the RTL's mirrored profile table matches the GENERATED table, and every
  `SCALE` really is a power of two;
* 24 comparisons — eight points across each of the three profiles' ranges;
* a `w` outside `[wmin, wmax]` takes the boundary's depth, and each clamp is
  counted;
* every profile pins its near plane at exactly `0xFFFFFF`;
* the far floor is the table's own non-zero value.

Evidence: 32 vertices, 1 near-clamped, 1 far-clamped, 0 saturated, 0 refused.
