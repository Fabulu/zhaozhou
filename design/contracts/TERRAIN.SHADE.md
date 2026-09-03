# Contract — TERRAIN.SHADE (Terrain base light)

> Ledger: `design/blocks.yml` · gpu clock · maturity REFERENCE_COMPLETE
> RTL: not built
> Reference: **`zref::render::shade_flat_tri_dir`** — the ratified law.
> `reference/include/zref/zref_terrain_shade.hpp` is a THIN VIEW onto it,
> exposing only what that function does not (squared norm, dot product,
> degeneracy predicate, light constants).

## Purpose and exclusions

TERRAIN.SHADE turns a triangle's face normal into a light term: `dot(n, L)/|n|`
in s1.15, once per triangle, reused by every fragment of it.

**Written 2026-09-03.** The reason it exists is a finding, not a feature
request: **production terrain has no lighting path at all.**
`TERRAIN.NORMALS` computes face normals and is UNIT_VERIFIED at 41,731 checks,
but nothing in `design/prod_manifest.yml` consumes them, `TERRAIN.PROJECT` has
no colour port, and the composed shell contains no terrain at all
(`zhao_shell_fit.qsf` says so in a comment). The normals are computed and
thrown away.

The owner brief (`reports/BRO-20260903-NORMALMAP-AND-ANIMATION-PATH.md`) is
explicit about what follows from that:

> The crucial discovery is that the 10-DSP piece is not really a normal-map
> expense. Production terrain currently has no proper lighting path at all
> ... **TERRAIN.SHADE is necessary whether you use normal maps or not.**

**This block is therefore NOT part of the normal-map cost**, and must be
measured separately from `TERRAIN.NORMALMAP` so the detail organ's delta cannot
be confused with the cost of the terrain finally being lit.

**Exclusions, each a specific refusal:**

* **No detail normals.** That is `TERRAIN.NORMALMAP`, deliberately a separate
  block with a separate budget so it can be cut cleanly.
* **No normal recomputation.** It consumes `TERRAIN.NORMALS`' output unchanged.
* **No fog, tint or ambient application.** Ambient is added where the shade
  meets colour — see Notes.
* **No per-fragment work at all.** Its whole economy is that the expensive
  arithmetic happens once per triangle.

## The algebra, and why it is affordable

    dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
                            \___________/     \__________/
                             THIS BLOCK        TERRAIN.NORMALMAP

`TERRAIN.NORMALS` emits one face normal per **triangle**, so the square root
and the divide — all of the expensive arithmetic — happen once per triangle and
every fragment reuses the answer. That split is the entire reason a per-pixel
normal map is affordable on this machine.

## Input and output packet layouts

In, ready/valid, one per triangle:

| field | width | meaning |
|---|---|---|
| `n_x_i` `n_y_i` `n_z_i` | signed 32 | face normal, Q16.16, **un-normalised** |
| `degenerate_i` | 1 | `TERRAIN.NORMALS` said the triangle has no normal |
| `sun_x_i` `sun_y_i` `sun_z_i` | signed 16 | s1.15 unit, **from** the surface **toward** the light |
| `src_id_i` | 16 | rides the packet |

Out:

| field | width | meaning |
|---|---|---|
| `base_o` | signed 16 | s1.15, `dot(n,L)/|n|`, **sign preserved** |
| `base_valid_o` | 1 | |

**The sign is kept, not clamped.** A face turned away from the sun produces a
negative base, and `TERRAIN.NORMALMAP`'s per-fragment term is *added* to it.
Clamping here would discard exactly the information the addition needs. The
clamp belongs where the shade is packed to unit8.

## Backpressure rules

Ready/valid. **The throughput point is an open question and is the first thing
to measure** — see Synthesis below.

## Memory ownership

None. It reads no memory and writes none.

## Q formats and rounding

* face normal in: **Q16.16, un-normalised**, exactly as `TERRAIN.NORMALS` emits
* sun direction: **s1.15** unit
* `base_o`: **s1.15**, saturating at ±32767

One rounding per result, **round-half-up** (`spec/qformats.md` §3). A `>>>`
shift **floors** and therefore disagrees on every negative value, which is
half of what this block produces. The oracle owns the rounding in
`zref::terrain::rshift_round`.

## Latency (fixed or variable)

`fixed`, and the number depends on the throughput point chosen below.

## Overflow and malformed-input behaviour

* **A degenerate triangle has no direction to be lit from.** `base_o` is zero,
  which means its fragments receive ambient only — a dark face, never a bright
  one. Counted.
* **The sum of squares must be accumulated in unsigned 64.** A Q16.16 component
  at the fx16 rail squares to 2^62 and three of those reach 1.38e19 against a
  signed-64 maximum of 9.22e18. The first oracle added them signed and was
  undefined behaviour on input `TERRAIN.NORMALS`' contract says is reachable
  inside the domain. **In RTL this is a silent wrap producing a small length
  and therefore a huge, wrong shade.**

## Scalar reference function

**`zref::render::shade_flat_tri_dir`** is the law, and it already existed.
`reference/include/zref/zref_terrain_shade.hpp` is a thin view onto it.

The first version of this contract cited a `zref::terrain::shade_base` that
**re-implemented** the law with a different square root, divide, Q format and
clamping — and its tests passed because they compared the duplicate against
itself. The ledger's V17 check then caught the citation when the duplicate was
removed, which is the drift guard working twice over.

## Directed tests

**`tests/terrain/terrain_shade_oracle.cpp` — WRITTEN**, 12 checks covering the
law: the fx16 rail not overflowing, round-half-up at both signs, a flat face
under an overhead sun being fully lit, a degenerate triangle shading to
ambient, ambient being an addend, and multiple suns saturating.

**The overhead-sun case exists because the draft RTL failed it.** Its divider
ran 32 steps over a 64-bit numerator, yielding quotient bits 63..32 when the
true quotient is always below 2^15 by Cauchy–Schwarz, so `base` came out **zero
for every realistic triangle** and the whole island would have shaded to
ambient — silently, with every gate passing.

**Planned and not written:** an RTL differential against the oracle across the
legal normal and sun space, once RTL exists.

## Randomized differential tests

Planned. Bias toward near-degenerate triangles and near-rail components, since
those are where both the divide and the accumulation break.

## Integration capture cases

None on hardware. **And one gate before RTL, which is the art law rather than a
process step**: the amended oracle goes into the ZRef renderer and the owner
**looks at the island under a moving sun at 240p** before this block is built.
A still frame will not do — see the zenith note in `TERRAIN.NORMALMAP`.

## Synthesis / resource ceiling

Estimated ~730 ALM, **10 DSP**, 1 M10K at II=1.

**The owner brief challenges that number and the challenge must be tested
before the DSPs are spent:**

> The proposed TERRAIN.SHADE spends about 10 DSPs to sustain one triangle per
> clock, but its producer is described as delivering roughly one triangle every
> three clocks. That suggests a very worthwhile Pareto test: one fully
> pipelined II=1 shade point; one time-shared II=3 shade point.

So **fit both**, and record them as two rows. If II=3 keeps up with the real
producer, the base light costs materially fewer DSPs.

**And the ordering rule that goes with it:** if DSPs get tight, lower *this*
block's throughput point — do **not** cut `TERRAIN.NORMALMAP`'s 2-DSP delta
first. That runs per fragment and genuinely needs the throughput; the unused
parallelism is here.

## Notes

Ambient is **added** where the shade meets colour, per `SetEnvironment 0x0311`
(`spec/sky_and_beams.md` §4a), which carries ambient as a colour beside the
sun. An earlier draft made it a *floor* and argued the case in a comment; that
was a terrain header re-legislating ratified law, and it is corrected. The
argument for a floor is recorded in the brief rather than enacted.

This block does not exist in `design/prod_manifest.yml` yet. Adding it is what
finally gives `TERRAIN.NORMALS` a consumer.
