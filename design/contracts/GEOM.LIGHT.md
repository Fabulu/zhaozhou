# Contract — GEOM.LIGHT (Vertex lighting, one vocabulary)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::render::shade_flat_tri_dir` is the ratified law — see below

## Purpose and exclusions

GEOM.LIGHT turns a **world normal** and the environment into a **vertex RGB**.
One block, one law, three different normal producers.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R2**, whose finding
is that the lighting seam is unfinished for everything except terrain — and
terrain only got caught this session:

* `GEOM.VDECODE` decodes a bind-space normal and says it *"travels on a
  parallel lighting path"*;
* **`GEOM.SKIN` transforms positions only**;
* `zhao_geom_project.sv` takes positions, view and source id — **no normal, no
  colour, no light set, no tint, no fog**;
* `SKIN.NORM` and `CREATURE.LIGHT` exist in a report, not as blocks.

**So the ledger's description of `GEOM.PROJECT` as "projection + lighting" is
aspirational. The source implements projection.** This contract exists so that
sentence becomes true somewhere specific.

### The route this block completes

    VDECODE normal
       |- rigid normal transform        (static meshes)
       |- two-weight SKIN.NORM          (creatures)
       '- TERRAIN.NORMALS face normal   (terrain)
                    |
              world normal
                    |
                GEOM.LIGHT   <-- this block
                    |
                vertex RGB
                    |
        PROJECT -> CLIP -> ATTRSTEP -> fragment

**Exclusions, each a specific refusal:**

* **No normal production.** Three different producers feed it; that is the
  point of the split.
* **No per-fragment work.** This is vertex lighting. `TERRAIN.NORMALMAP`'s
  per-fragment detail is a separate, cuttable organ.
* **No shadowing.** Contact shadows are geometry (audit R8), not a light term.
* **No new arithmetic.** See below — the law already exists.

## THE LAW ALREADY EXISTS AND MUST NOT BE RE-IMPLEMENTED

`zref::render::shade_flat_tri_dir` is described in
`reference/src/zrender/internal.hpp` as **"The ONE flat-shade law"**, and the
golden captures pin it. It is:

    fx,fy,fz   Q16.16 face normal, one rescale(.,16) per lane
    light      Q16.16 hand-normalised unit (1,2,1)/sqrt(6)
               kLightX 26758, kLightY 53521, kLightZ 26758
    ndot       __int128
    nmag2      uint64
    shade      div_rhu_s128(ndot, isqrt_u64(nmag2))   -- ONE rounding, Q16.16
               clamped to [0, 0x10000]; zero area returns 0

**On 2026-09-03 a second implementation of this was written by accident** —
different square root, different divide, s1.15 output — and its twelve checks
passed because they compared the duplicate against itself. That is why this
contract states the law rather than paraphrasing it, and why the oracle for
this block must be a **thin view** in the manner of `zref_terrain_normals.hpp`:
*"a THIN view onto an existing ratified law, not a second implementation of
it."*

Two consequences of reading it properly, both of which nearly shipped wrong:

1. **The light is Q16.16, not s1.15.** Assuming s1.15 is a factor of two in the
   result and looks like a tuning problem rather than a units problem.
2. **The clamp to `[0, 0x10000]` is INSIDE the ratified function.** Any additive
   term — a detail normal, a second sun, a spell light — must be summed
   **before** that clamp. A face slightly turned from the sun has a small
   negative base and relief on it should still catch light; adding to an
   already-clamped zero can neither darken nor brighten from below.

   **THE OWNER DECISION THIS BLOCK IS BLOCKED ON:** either the ratified law
   grows an **unclamped variant** that both it and the additive terms consume,
   with the clamp moving to the single point where shade becomes colour — or
   the additive term is passed **into** it so one function owns sum and clamp.
   **(a) is recommended**: it keeps detail out of a function terrain and
   creatures share, and it is a pure refactor whose golden CRCs must not move.

## Input and output packet layouts

**In**, per vertex, ready/valid: world normal (Q16.16), base colour, the
environment record (`SetEnvironment 0x0311`: sun direction and colour, ambient,
tint, fog), and `src_id`.

**Out**: vertex RGB, ready/valid.

## Backpressure rules

Ready/valid. **`GEOM.SKIN` fits at 89.65 MHz with 9 DSPs and one weighted
vertex per 12 clocks, and its contract says nothing more may be bolted onto its
output.** So this block sits beside it in the vertex path at a rate the skinner
already sets — it does not need to be faster than its producer, and that is the
lever if DSPs get tight.

## Memory ownership

None. The environment arrives as a record; light sets, if bounded top-K ever
lands (audit R9), arrive as a per-creature context rather than a memory this
block walks.

## Q formats and rounding

Inherited, and inherited exactly: Q16.16 normal, Q16.16 light, Q16.16 shade
with **one rounding per result**, round-half-up per `spec/qformats.md` §3. A
`>>>` shift floors and disagrees on every negative value, which is half of what
a turned face produces.

## Latency (fixed or variable)

`fixed`, once the throughput point is chosen. **Not chosen here** — see the
capacity note.

## Overflow and malformed-input behaviour

* **A zero-length normal is degenerate**: shade 0, matching the ratified law's
  `nmag2 == 0` guard. Counted.
* **The squared norm accumulates in UNSIGNED 64.** Three Q16.16 components at
  the fx16 rail reach 1.38e19 against signed 64's 9.22e18, and the rails are
  reachable inside the domain. Signed accumulation is undefined behaviour in
  C++ and a **silent wrap in RTL — a small norm and therefore a huge, wrong
  shade.**
* **Several light terms saturate rather than wrap.** Two suns on one face is
  brighter, never darker.

## Scalar reference function

`zref::render::shade_flat_tri_dir` is the law. A thin view exposing the
unclamped result is **PLANNED AND NOT WRITTEN** and depends on the owner
decision above.

## Directed tests

**PLANNED AND NOT WRITTEN**, and the one that matters most: **RTL against the
ratified law over the legal normal and light space**, not against a restatement
of it. Plus the degenerate normal, the fx16 rail, and saturation with several
terms.

## Randomized differential tests

Planned, biased toward near-degenerate normals and near-rail components, since
those are where the norm and the divide break.

## Integration capture cases

None on hardware. **The composed case is the point**: terrain, a rigid mesh and
a skinned creature lit by the SAME block, differing only in normal producer. If
that composition needs three different colour laws, this contract has failed.

## Synthesis / resource ceiling

**Not estimated, deliberately.** The audit calls lighting *"the largest
potential new resource expense"* and also *"highly shareable and sequenceable"*.
The controlling sentence is: **the accumulator does not need ten permanent light
engines merely because ten light terms are legal.** Sequenced accumulation at
the skinner's own rate is the default; parallel engines are a measured
escalation, not a starting point.

## Notes

`GEOM.SKIN` outputs positions and not normals; normal skinning is
reference-only today. **`SKIN.NORM` is a prerequisite for the creature path**
and is not this block. The specification inconsistency noted in the docket
applies to it: the old law transformed each normal by each bone and blended
scalar Lambert results, while the live reference blends the normal *vector*,
renormalises, then takes Lambert — and **the live reference becomes the law.**
