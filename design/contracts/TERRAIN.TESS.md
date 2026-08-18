# Contract — TERRAIN.TESS (Crack-safe tessellator)

> Ledger: `design/blocks.yml` · owner ZH-034 · phase 6 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` (world-identity wave). This block turns a
> composed lattice into triangles for BOTH island surfaces — top and underside —
> with crack-safe stitching; rim walls belong to FORGE.CLIFF.

## Purpose and exclusions

Subpatch tessellation at crack-safe resolution with stitching and geomorph
between levels, emitting triangles ONLY for SOLID cells (terrain_rules
§3.2/§3.5).

Implemented as `fpga/rtl/terrain/zhao_terrain_tess.sv`.

Exclusions: no rim walls (FORGE.CLIFF), no normals (TERRAIN.NORMALS), no LOD
decisions (TERRAIN.LOD decides; this block obeys), and **no per-vertex UV**. The
reference computes terrain UV in its DRAW loop from a patch-level `top_shift`,
and terrain_rules §6.2's mirrored-repeat fold is the TMU sampler's law, not the
tessellator's; emitting UV here would fix an underside/wall UV law that
FORGE.CLIFF owns. Recorded as an exclusion rather than left to inference.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset returns the block to IDLE with no in-flight subpatch, an empty output
register and zeroed counters. No clock-domain crossing lives in this block.

## Input and output packet layouts

`patch_state` + `lod_target`, one subpatch of one surface, ready/valid:

| field | width | meaning |
|---|---|---|
| `job_ox_i` `job_oz_i` | 6 | subpatch cell origin, a multiple of 8, 0..24 |
| `job_level_i` | 2 | own level; stride = `1 << level` |
| `job_lvl_nz_i` `pz` `nx` `px` | 2 each | neighbour levels, terrain_rules §6.6 side order −z, +z, −x, +x |
| `job_morph_i` | 17 | geomorph factor, Q16; above 65536 is clamped and counted |
| `job_surface_i` | 1 | 0 = top, 1 = underside |
| `job_dual_i` | 1 | 0 = the legacy single-surface page |
| `job_src_id_i` | 16 | rides the mesh |

Lattice read port, **registered — the datum is present the cycle AFTER the
request**: `lat_req_o`, `lat_vi_o`/`lat_vj_o` (6), `lat_surface_o` (which height
plane to return), and in `lat_h_i`, `lat_wx_i`, `lat_wz_i` (signed 32).

Cell-state read port, registered the same way: `cs_req_o`, `cs_ci_o`/`cs_cj_o`
(5), `cs_substance_i` (2; 0 = SOLID per §3.3).

`terrain_mesh` out, ready/valid — **exactly TERRAIN.NORMALS' input packet**:
`ax_o`…`cz_o` (nine signed 32, fx16 world units), `surface_o`, `src_id_o`. The
two blocks are wired port-for-port in `tests/terrain/terrain_tess_normals.cpp`
with no adapter; if that stops being true, that file stops compiling.

Plus `terrain_triangles_emitted_o`, `subpatch_rejected_o`, `lod_clamped_o`
(32 each), `job_reject_o` (1-cycle pulse) and `idle_o`.

## Backpressure rules

Ready/valid on the job port and the mesh port. A stalled consumer stalls the
address generator without state loss: the block simply does not issue the LAST
read of a triangle while the output register is occupied, so there is never an
in-flight read whose datum has nowhere to go. Four stall schedules, including
one that stalls 31 cycles in 32, ride the directed suite on both the plain and
the stitched path, and the random lanes stall on a varying schedule throughout.

## Memory ownership

Read-only on the two lattices and the cell-state plane, through the two
registered ports above. **Writes nothing anywhere** — it emits stream packets
only.

## Q formats and rounding

Vertex positions are lattice values **verbatim**: the tessellator never
re-rounds a height it did not change. The ONE rounding in the block is the
geomorph blend, and it is derived rather than chosen — see Notes law 5.

`coarse_height(ha, hb) = ha + rescale(hb - ha, 1)` is §4.3's own interpolation
of the next-coarser cell at a vertex the coarser level does not carry: at
u = v = 1/2 the two-MAD form collapses over its common denominator to a single
round-half-up halving. `morph_height(h, hc, m) = h + fx_mul(m, hc - h)` is
§4.3's shape too — an exact add of a rounded delta, never a rounded sum of two
rounded terms (qformats §3). **m = 0 gives h and m = 65536 gives hc
bit-exactly**, which is what "factor 0/1 = exact levels" means in gates.

**The geomorph blend is convex, so an emitted height can never itself rail.**
The saturating path that does exist is inside `coarse_height`, when the two
coarse parents are a whole fx16 word apart and the half-difference leaves the
word; the randomized lane B reaches it and the block matches the reference
there. That is stated because a lane asserting "the output must rail" would
have been asserting something impossible.

## Latency (fixed or variable)

Variable. Per subpatch: a fixed **65-cycle cell-state prologue** (the 8×8
solidity read once, dual pages only), then 3 lattice reads per triangle without
geomorph and up to 9 with it, plus one cycle per void run-cell skipped.

Bounded per subpatch at ≤ 128 triangles on either surface — an unstitched level
0 subpatch is 2 × 8 × 8, and the annulus is always fewer.

## Target throughput

The ledger asks for "1 emitted vertex per clock", and this block issues exactly
one lattice read per clock in steady state, which is one vertex per clock —
**3.00 cycles per triangle.**

**MEASURED, and the amortized number is stated rather than the flattering one.**
`terrain_tess_directed` §10 prints it: a level-0 subpatch with no morph takes
**456 cycles for 128 triangles = 3.56 cycles/triangle = 0.84 vertices/clock**.
The gap is the fixed 65-cycle cell-state prologue plus the drain; 456 − 65 −
drain is 384 = 3 × 128 exactly, so the steady state does meet the rate and the
subpatch-level rate does not.

**With geomorph the rate is worse and that is inherent:** a morphed vertex needs
its two coarse parents, so up to 3 reads per vertex. Measured at factor 0.5,
level 0: **936 cycles for 128 triangles = 7.31 cycles/triangle.** Morph = 0 is
the steady state (a subpatch is only mid-morph during a transition), but a frame
in which many subpatches are transitioning at once costs this and no contract
should pretend otherwise.

## Overflow and malformed-input behaviour

**Input domain:** `job_ox_i`/`job_oz_i` are multiples of 8 in 0..24; levels are
2-bit; the lattice is 33×33 with a **power-of-two pitch**. The pitch requirement
is not an assumption about content — terrain_rules §2.1 makes the packer ASSERT
that a patch envelope equals `origin + coords × 32 × pitch` exactly, and §4.3 is
itself written in the shift form ("cx = (wx − env_x0) >> pitch_log2 — shift, no
division"). Heights are the whole fx16 word.

- **A lod_target above the legal resolution set is UNREPRESENTABLE**, not
  clamped: the level set is {1, 2, 4, 8} because a subpatch edge must land on
  lattice vertices and those are the divisors of 8, and a 2-bit field encodes
  exactly them. That is better than a clamp-and-count, and it is why
  `lod_clamped_o` counts only the one lod_target lane that CAN be out of range —
  the morph factor, clamped into [0, 65536].
- **A subpatch whose cells are all void emits nothing.** Legal and common on
  sparse islands, not an error.
- **An underside job on a legacy single-surface page emits nothing** — that page
  has no modelled underside (§3.1 option (a)).
- **A coarsened subpatch containing a void cell is REJECTED**, loudly, with
  `job_reject_o` and `subpatch_rejected_o`. See Notes law 3.

## Counters and traces

`terrain_triangles_emitted_o` counts emitted triangles; the top/underside split
rides `surface_o` on the stream. `subpatch_rejected_o` and `lod_clamped_o` are
the two degrade counters. All three are pinned by directed cases.

## Scalar reference function

`zref::terrain::tessellate` in `reference/include/zref/zref_terrain_tess.hpp`,
with `coarse_height` and `morph_height`.

The ledger names it `zref::TerrainTess`; it lives under `zref::terrain::` as
functions, alongside `column_query` whose triangulation it shares, and for the
same reason TERRAIN.PATCH's does — that namespace is where the terrain laws
already are.

That header is **part view, part definition, and it says which at every line.**
The cell split, the emit order, both windings, the void rule's `dual` guard, the
scan order and the geomorph target are views onto ratified law. The level
encoding, the annulus, the all-solid rule at stride > 1, the void-stitch reject
and the interior-only morph are defined there and are marked as chosen.

## Directed tests

`tests/terrain/terrain_tess_directed.cpp` — **6,751 checks**, in four layers:

1. **The unstitched oracle IS §4.3**, restated independently in the test from
   the spec text — the fixed diagonal, `(i00,i11,i10)` then `(i00,i01,i11)`, the
   underside's b/c swap, z-then-x — at every stride and on both surfaces. Plus
   the winding as a SIGN: every top triangle of a flat subpatch is clockwise in
   (x,z), every underside triangle counter-clockwise.
2. **The geomorph target IS `column_query`**: `coarse_height` is compared
   against `zref::terrain::column_query` evaluated on a real coarse cell over
   the whole 11 × 11 height cross-product, with the sweep asserting it actually
   sampled odd differences (where the rounding shows), plus both halves of the
   round-half-up boundary by hand.
3. **The RTL against the oracle:** every stride on both surfaces; all four patch
   corners at every level, stitched and not; **all 256 neighbour-level
   combinations at all 4 own levels** (1,024 jobs, 670 of them stitched), each
   also checked to tile its 16 m square exactly — total signed area −512, no
   gaps, no overlaps, no triangle with the wrong winding; void cells at every
   stride, breached as well as authored; the all-void subpatch; the reject path
   and the same subpatch unstitched; the legacy page including **all 256
   neighbour combinations on it**; geomorph at six factors × four strides × both
   surfaces × stitched and not, the factor-0 identity, the rounding boundary on
   the RTL, and the clamp; four backpressure schedules; the counters, the source
   id, the surface tag and idle; and the throughput measurement.
4. **Crack-safety read off the RTL's own emitted geometry:** for all 16 level
   pairs, the two subpatches sharing an edge use the IDENTICAL vertex set on it,
   and that set is exactly the coarser side's stride.

## Randomized differential tests

`tests/terrain/terrain_tess_random.cpp` — **2,277 checks** over 800 jobs and
23,171 triangles (3× nightly). Two lanes:

- **Lane A, lattice-shaped:** an authored island — height16 relief over a deep
  keel **plus the sub-height16 fx16 detail the §3.4 field chain leaves behind**,
  scattered void and breached cells, neighbour levels usually equal or one apart
  the way a projected-error selector produces them.
- **Lane B, domain-limit:** heights spread across the fx16 word, fully random
  neighbour levels, a fifth of the pages legacy, the whole morph range.

Both lanes assert they reached their states: lane A must exercise the annulus,
void run-cell skips, the reject path, geomorph actually moving vertices, and
undersides, and must NEVER record a saturation; lane B must record saturations,
must exercise the annulus and geomorph, and must tessellate legacy pages.

**Lane A carries fx16 detail because a mutation proved it had to.** Filling the
lattice only on the height16 grid makes every geomorph parent difference EVEN,
so the halving never has a remainder and a truncation in place of round-half-up
survives the entire suite. A composed lattice is genuinely not on that grid —
`live_top = base<<8 + the fx16 field chain` — so the old fill was both
unrealistic and blind.

## Formal properties

**None, deliberately** — and this block is the one where that needs an argument,
because the ledger note says "Crack-safety is a formal candidate (stitch
invariants)".

The stitch invariant the contract asks for is "for any two adjacent subpatches
at legal level pairs, the shared-edge vertex sets are identical". As built that
is not an emergent property to be discovered by a solver: it is a one-line
consequence of `edge_stride[side] = 1 << max(own, neighbour)` being SYMMETRIC
and of the annulus's outer walk using only that side's coarse vertices. A
bounded proof would restate the `max`. What is worth checking is that the
implementation actually has the property, and the directed suite checks it the
strong way — on real emitted geometry, for all 16 level pairs, comparing the
vertex sets two independently-run subpatches put on their shared line.

There IS a property here a solver could own that testing cannot: **exact
tiling** — that the emitted triangles cover the subpatch with no gap and no
overlap for every one of the 1,024 level/neighbour combinations. The directed
suite settles it by signed area over all 1,024, which is a complete check of the
finite space, not a sample. A solver would add nothing.

## Synthesis / resource ceiling

**Not synthesized.** `fpga/files.qip` is untouched and this block has never been
through Quartus. Nothing here has run on hardware. `geometry_mantle` group
(charter §25, 20% ceiling) when it is.

## Integration capture cases

**`TERRAIN.TESS` → `TERRAIN.NORMALS` is built and green**:
`tests/terrain/terrain_tess_normals.cpp`, **41,731 checks**. Every subpatch of a
domed island, every level, both surfaces, stitched and not, with geomorph on
half of them: 5,152 top and 5,152 underside triangles through both real blocks,
port-for-port, no adapter.

Two assertions, and the second is the point:

1. Every normal equals the ORACLE's normal of the triangle TESS actually
   emitted, and the degeneracy verdict and source id survive the chain.
2. **Every top triangle's normal has ny > 0 and every underside triangle's has
   ny < 0.** Neither block can make that statement alone. TERRAIN.NORMALS emits
   an unnormalised cross product and does not take its absolute value, so a
   flipped winding in TERRAIN.TESS points the normal into the ground and the
   island shades black — the same symptom the rescale-32 defect produced, and
   the last symptom anyone would attribute to the tessellator. The mutation
   sweep confirms it: flipping the winding turns exactly 10,304 of these red.

Not yet composed: `TERRAIN.PATCH` upstream (the composed-height cache that would
sit between them does not exist), `TERRAIN.LOD` (does not exist), `FORGE.CLIFF`
downstream. The Phase-6 gate captures — Duo island from two cameras with all
stitch patterns crack-free, and the breach hole + rim capture — remain open.

## Notes

**LAWS FOUND** (each cited, none invented): the fixed i00–i11 diagonal and the
emit order §4.3 pins to `draw_heightfield`; the y-up winding
(`reference/src/zrender/terrain.cpp` says it in as many words, and
TERRAIN.NORMALS already depends on the sign); the underside as the same pair
with b and c swapped, which that same file labels "TERRAIN.TESS law"; void cells
emitting no surface with the test guarded by `dual`; z-then-x scan order; and
the geomorph target as §4.3's own interpolation.

**LAWS CHOSEN, NOT FOUND.** Each is also argued in the RTL header and the
oracle.

1. **The level ENCODING.** The level SET is forced, not chosen: charter §11.1
   gives 8×8-cell subpatches, a subpatch edge must land on lattice vertices, so
   the stride must divide 8, and the divisors of 8 are exactly {1, 2, 4, 8}.
   What is chosen is level 0..3 with stride `1 << level` — which also makes an
   illegal resolution unrepresentable rather than clampable. **Rejected
   alternative:** arbitrary run counts, which do not close on the lattice and
   square the stitch case matrix — terrain_rules §3.1(d)'s own objection to
   column runs.
2. **The ANNULUS.** `edge_stride[side] = 1 << max(own, neighbour)` is symmetric,
   so both sides of a shared edge compute it identically; the ring's outer walk
   uses ONLY those coarse vertices. **Rejected alternative 1:** vertex snapping
   (the finer side keeps its extra boundary vertices and slides them onto the
   coarse segment) — that leaves T-junctions, and a fixed-point rasterizer
   cracks at a T-junction by a pixel, which is the 2026-08-15 seam-crack defect
   class in `design/contracts/GEOM.CLIP.md`. **Rejected alternative 2:** a
   one-cell strip per side, which is simpler right up until two adjacent sides
   are both coarsened — then the corner square's other two corners lie on the
   two boundary lines and NEITHER is in its side's coarse set. The annulus has
   no corner case at all, because a subpatch corner is index 0 of both its
   sides' coarse sets.
3. **A COARSENED SUBPATCH CONTAINING A VOID CELL IS REJECTED**, counted, and
   emits nothing. The ring's fans are not aligned to run-cells, so honouring a
   void inside one needs a conservative bounding-box cell scan per fan — up to
   64 reads for a fan that emits two triangles — to buy geometry no
   projected-error LOD selector should ask for, since a breached subpatch is
   exactly the one §4.4 keeps fine. **Rejected alternatives:** roofing over the
   hole (silent, and terrain_rules §3.5 says a breach shows sky), or the per-fan
   scan. **This creates an obligation and it is stated rather than hidden:
   TERRAIN.LOD must not coarsen a neighbour past a subpatch that carries void
   cells.** Rejecting leaves a hole in the island, which is bad — but it is
   LOUD, and a silently roofed breach is worse.
4. **The all-solid void rule at stride > 1**: a run-cell is emitted iff EVERY
   one of the stride × stride patch cells it covers is SOLID. **Rejected
   alternative:** emit if any is solid. The cost is stated: at a coarse level one
   breached cell erases up to 8×8 cells of surface.
5. **GEOMORPH APPLIES ONLY STRICTLY INSIDE THE SUBPATCH, and this one is a
   CONTRACT GAP rather than a preference.** A boundary vertex is shared with a
   neighbour that has its own morph factor; moving it needs both sides to agree,
   and `lod_target` as specified carries **one factor and four neighbour
   LEVELS** — it cannot express the neighbour's factor at all. Leaving boundary
   vertices unmorphed makes crack-safety hold unconditionally, for every level
   pair and every factor pair, with no cross-subpatch agreement of any kind, and
   the directed suite asserts that no boundary vertex ever moves while interior
   ones genuinely do. **The cost is a shallow crease at subpatch borders during
   a transition, bounded by the level's own height deviation.** **Rejected
   alternative:** morphing boundary vertices too — the textbook form, strictly
   better looking, and not implementable correctly until `lod_target` gains a
   per-edge morph factor. **That is an amendment to this contract's own
   `lod_target` layout, and it is left open for whoever ratifies it, not decided
   here.**

**MUTATION-CHECKED.** Seven defects were injected one at a time, each proved to
have relinked by hashing the test binaries before and after, and each confirmed
to turn the required lanes red before being reverted:

| mutation | directed | random | composition |
|---|---|---|---|
| triangle winding flipped | 2174/6741 red | 572/2264 red | **10304/41731 red** |
| a height sample off by one cell | 1820/6741 red | 485/2264 red | green |
| an LOD segment stride off by one | 1372/6741 red | 672/2264 red | 192 red |
| the edge stride ignores a coarser neighbour | 782/6741 red | 414/2282 red | 96 red |
| a breached void column treated as solid | 5/6741 red | 170/2315 red | green |
| the geomorph rounding truncated, not round-half-up | 29/6751 red † | 213/2277 red | green |
| the void-stitch reject accepted instead of rejected | 3/6751 red | 267/2410 red | not run |

† **initially GREEN in the directed lane**, because every directed lattice was
height16-quantised and every geomorph parent difference was therefore even. The
lane was fixed (fx16 detail on the lattices, plus an explicit ±1 rounding-
boundary case read back off the emitted mesh) and the mutation re-run. The
composition lane staying green on three of these is correct and not a gap: it
exists to catch the two blocks DISAGREEING, and a TESS-internal addressing
error is the differential lanes' job.

**Two mutations were prevented from proving nothing.** The first attempt at the
"height sample off by one" mutation reported all three lanes PASSING with an
IDENTICAL binary hash — the build had not picked the edit up. The harness now
writes the source twice with a build between, on both the apply and the revert
side, and prints the hash every time; an unchanged hash means the mutation did
not run, and is treated as a failed run rather than a green one. The same
discipline caught a mutated binary left in `build-verify` after a revert
reporting its failures as the baseline's.

**No mirror-fold mutation exists, deliberately.** terrain_rules §6.2's
mirrored-repeat texel fold is TEXTURE.MOSAIC's law and lives in the TMU sampler,
not in this block or in TERRAIN.PATCH — neither of which emits UV at all (see
Exclusions). There is nothing here to mutate, and inventing a fold to mutate
would be worse than saying so.
