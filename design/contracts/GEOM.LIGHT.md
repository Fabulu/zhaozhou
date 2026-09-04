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
2. **The clamp to `[0, 0x10000]` was INSIDE the ratified function.**
   **RULED AND IMPLEMENTED, D-1, 2026-09-03**
   (`reports/OWNER-RULINGS-20260903-FUNDAMENTALS.md`):
   `shade_flat_tri_dir_unclamped` is now the signed primitive and
   `shade_flat_tri_dir` is a bit-identical `clamp01` wrapper around it. The
   goldens are unmoved, which is how the refactor is known to be correct.

   **AND THE REFINEMENT THAT CORRECTS AN EARLIER DRAFT OF THIS FILE.** It said
   "additive terms must be summed before the clamp". **That is true only WITHIN
   ONE LIGHT.** The ruled composition:

       for each independent light i:
           raw_i = shade_flat_tri_dir_unclamped(n, L_i)
           ndl_i = clamp01(raw_i + normal_detail_i)   // detail is THIS light's
           rgb  += light_colour_i * ndl_i
       rgb += ambient + spill
       final = saturate(rgb)

   A detail normal may brighten a face turned slightly from **that same** sun,
   so base and detail combine before clamping. But **a second sun or a fireball
   is an INDEPENDENT light: its negative dot must become zero and must not
   subtract illumination another light contributed.** So the clamp is **once
   per light**, not once per fragment. Ambient, spill and global flash are
   colour contributions, not signed directional terms.

   The earlier phrasing would have been wrong the moment a second light
   existed.

## THE ADDITIVE EMISSION TERM — PROVISIONAL, 2026-09-03

**Owner: *"It's fucking beautiful we must have it."* And, on the budget:
*"Our budget is fucked but we can always go back on this and we might make a
miracle happen."***

So this is **written into the specification provisionally**, with the revert
path designed in rather than discovered later. It is in the contract so nobody
designs `CREATURE.LIGHT` around its absence; it is marked provisional so nobody
treats it as load-bearing.

### What it fixes

Creature point lights were **multiplicative only** — texel colour times a
per-channel gain — so **a coloured light could only ever subtract.** A red
source on green pigment could only make olive. It could not make the pigment
glow red, because there was no term capable of adding energy.

### The law, and where it sits in the ruled order

The emission is a **per-source term accumulated into `rgb`**, and it saturates
**once at the end** with ambient and spill:

    for each independent light i:
        raw_i = shade_flat_tri_dir_unclamped(n, L_i)
        ndl_i = clamp01(raw_i + normal_detail_i)
        rgb  += light_colour_i * ndl_i          // multiplicative gain
        rgb  += emission_i     * ndl_i          // ADDITIVE, same response
    rgb += ambient + spill
    final = saturate(rgb)                        // ONCE, at the end

**Two things about that placement are not negotiable:**

1. **The additive sum uses the SAME per-source lambert × attenuation response
   as the multiplicative gain.** A flat additive lift raises lit and unlit
   faces equally and **flattens form** — which is exactly the failure the
   creature rig was rewritten to cure in August. The dot product and
   attenuation are already computed for the multiplicative term and are
   **shared**, which is also why the marginal cost is small.
2. **Saturation happens once, at the end.** Saturating per source would clip
   each contribution separately and **change the colour of an overlap** — two
   lights meeting would produce a different hue than either implies.

Bypassing the 1.0 gain ceiling **is the point**: a source strong enough to clamp
all three channels becomes a hue-neutral floodlight that erases its own colour
and everyone else's.

### Cost, measured on the prototype

**+3 MACs per source per evaluation** — the dot product and attenuation are
shared with the multiplicative term — **plus one saturating 3-channel add.** At
the ruled budget of four simultaneous sources with strongest-four selection,
that is **twelve adds at the limit**.

The application is **per fragment**, which costs **three extra interpolator
lanes** carrying the accumulated emission.

### Why per-fragment rather than per-face

Judged in `reports/CREATURE-LIGHT-ADDITIVE-COST-JUDGEMENT.md`:

* **timing is not the constraint.** `attrdiv_svc` is a shared tagged service, so
  three more attributes are three more tagged requests, not three more
  dividers, and nothing lengthens a combinational path. The critical path is
  Early-Z's presence lookup, elsewhere.
* **the real cost is ALM and M10K** — storage in `GEOM.PARAMBUF` and width in
  the fragment packet — on the axis already over budget. Counted, not waved
  through.
* **per-face saves the interpolant lanes, which are the cheapest part**, and
  pays with banding on triangle edges — a visible artefact on the exact feature
  whose entire justification is that it looks beautiful.

### THE CUT SEAM, so "going back on this" is cheap

**`emission_i = 0` is a bit-exact no-op.** With every source's emission zero,
the accumulated term contributes nothing and the result is identical to the
multiplicative-only path — which the prototype demonstrates: **CRC-identical
across all 22 subjects with the gate off.**

So reverting means **removing three interpolant lanes and one saturating add**,
and changes nothing else. No other block's behaviour depends on it, no packet
field is repurposed, and no law is rewritten. **That is what makes this safe to
adopt provisionally rather than a commitment that has to be honoured.**

### The saturation constraint, which is an artist-visible design input

From the prototype, and it is **not a tuning note**:

> at the pool core a strong emission can peg a channel — the prototype pegs red
> over a few hundred pixels at its strongest, where modelling survives only in
> green and blue. It holds at 384×240, and a harder emission tips into neon.

So emission strength is a **named, editable constant with a stated failure
mode**, and its shipped value is chosen **by looking at it in scene, at final
resolution, against what it sits on** — the art law, exactly. A value that
measures fine and pegs a channel is wrong.

### Status

**PROVISIONAL.** Adopted because the owner has seen it and wants it; costed
honestly rather than optimistically; and built so that the resource count
deciding against it costs one deletion rather than a redesign.

## FOG — RULED, D-5, 2026-09-03

`SetEnvironment` is **promoted from reserved to implemented** when this path
lands, and this block is where fog is computed.

**Do not carry an already-fogged vertex colour.** Carry two things:

* **unfogged lit RGB**
* a **fog factor**, computed once per vertex from the frozen view/fog law

The factor is transported through clipping and `GEOM.PARAMBUF` and interpolated
through `ATTRSTEP`, exactly like any other attribute.

### The one unified final ordering

| step | ordinary material | cel material |
|---|---|---|
| 1 | lighting | lighting |
| 2 | interpolate lighting + fog factor | interpolate lighting + fog factor |
| 3 | — | **toon quantisation** |
| 4 | texture/material combination | texture/material combination |
| 5 | **fog the final source RGB** | **fog the final source RGB** |
| 6 | framebuffer blend | framebuffer blend |

**Fog is applied to the final source colour before alpha or additive
framebuffer blending.** Fog-exempt classes — the sky family, HUD, and
deliberately emissive or additive effects — take an **explicit bypass** rather
than relying on a fog factor that happens to be zero.

### The two errors this ordering exists to prevent

1. **fog quantised into hard toon bands** — which is what fogging before toon
   does, and it is wrong precisely at distance where fog matters most;
2. **texture modulation multiplying the fog colour itself** — which is what
   fogging before the material combine does.

This closes a contradiction the audit found: the environment state and fog
arithmetic were both specified, `RASTER.FRAGMENT` assumed colour arrived
already fogged, and the actual projector had no colour input at all.

## LOCAL SPELL LIGHTS — RULED, D-6, 2026-09-03

**Yes.** *"A giant magical fireball does not illuminate only itself like a
pasted-on billboard."*

| receiver | v1 lighting |
|---|---|
| creatures and ordinary objects | deterministic bounded **top-K** coloured diffuse lights |
| terrain | sun + **broad local spill and global flash** — not every point light per fragment |
| particles and spell surfaces | emissive/additive materials + glow tags |

**The HPS may hold arbitrarily many emitters** and deterministically selects the
bounded set for visible receivers using intensity, projected importance,
authored priority, **stable source-ID tie-breaking, hysteresis and minimum hold
time**. Determinism is not decoration here — a light set that flickers between
frames is a creature that strobes.

For creatures and objects:

* **4** local lights guaranteed for a near/full-detail receiver;
* **6** targeted for hero and bosses;
* **8** is the descriptor maximum;
* ambient and broad coloured spill need **no** directional dot product;
* **first degradation** is dropping the expensive face-normal response on weaker
  lights, **then** dropping the weakest local light.

**One sequenced accumulator evaluates the selected terms. K legal lights does
not mean K permanently instantiated light engines** — that sentence is the
difference between this being affordable and being a shader core.

Terrain takes a **cheaper low-frequency representation**: broad spill around
nearby major effects and global flash envelopes for explosions and lightning.
**It does not evaluate every spark against every terrain fragment.**

**No dynamic shadow casting from these lights in v1.** The diffuse colour
response is the perceptual connection that matters.

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
and is not this block.

**The law it must implement was repaired on 2026-09-04** and now lives in
`spec/creature_rules.md` §2.x.1 rather than in a docket entry. The spec had
carried a *different* law as ratified — blend the two bones' clamped scalar
Lambert responses, no renormalisation — which its own oracle never implemented.
`skin_normal_lambert` blends the normal **vector**, renormalises once through
`isqrt_u64`, and takes Lambert last.

**Two consequences land on this contract:**

* **`SKIN.NORM` cannot be a widening of `GEOM.SKIN`.** The new law needs the
  normal transformed by both bones and renormalised — roughly 27 multiplies and
  a square root per vertex. `CREATURESANDLIGHTS` states that `GEOM.SKIN` fits at
  89.65 MHz with 9 DSPs and one weighted vertex per 12 clocks, and that
  **"nothing more may be bolted onto its output"**. So it is a separate block,
  and the struck law's whole appeal was that it required no such block at all.

* **This block receives an ALREADY-NORMALISED world normal, and that is what
  keeps its per-light cost at one dot.** The transform, blend and
  renormalisation happen once per vertex in `SKIN.NORM`; `GEOM.LIGHT` then
  spends three multiplies and one division per light. The reference calls
  `skin_normal_lambert` once per *light*, and the owner is explicit that
  **the hardware must not reproduce that structure** — copying it would put
  three square roots per vertex behind this block for no change in the answer.
  Bit-exactness is owed to the reference's **result**, not to how many times it
  recomputes the normal.
