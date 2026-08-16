# Contract — FORGE.CLIFF (Terrain cliffs, rims and skirts)

> Ledger: `design/blocks.yml` · owner ZH-067 · phase 6 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` §5 (world-identity wave). This block
> owns the island's silhouette: rim walls around bites and breaches, and
> LOD-seam skirts. The rim is where "more deformable than Sacrifice" is
> visible — treat this contract as identity-critical.

## Purpose and exclusions

Generate rim-wall geometry along SOLID↔(VOID/OUT) cell boundaries — one
textured quad per rim edge, top ends on the tessellator's stitched
composed-top edge vertices, bottom ends on the bottom lattice at the same
vertices — plus interior LOD-seam skirts (skirt depth derived from the
terrain LOD delta, never a tunable).

Exclusions: no top/underside surfaces (TERRAIN.TESS), no void/breach
decisions (TERRAIN.BAKE writes cell state; this block only reads it).

## Clock and reset semantics

`gpu` domain, synchronous reset to IDLE, stateless across frames.

## Input and output packet layouts

- `terrain_mesh` context (from TERRAIN.TESS): stitched edge vertex sets +
  per-subpatch level, cell-state plane handle.
- Output: triangle packets in the common geometry format, material = the
  tileset's strata/underside-reserved tiles (terrain_rules §6.6), source
  IDs tagged rim/skirt.

## Backpressure rules

ready/valid; stall-safe.

## Memory ownership

Read-only (lattices, cell state). Stream output only.

## Q formats and rounding

Wall vertex positions are lattice values verbatim (fx16, no re-rounding).
Texture U = accumulated rim length / STRATA_M, V = (top − y) / STRATA_M
(terrain_rules §5), computed with qformats §3 single-rounded fx_mul against
the reciprocal of the constant STRATA_M (exact table constant, not a
runtime division).

## Latency (fixed or variable)

Variable, bounded by the emission clamp below.

## Target throughput

1 wall quad per 2 clocks (2 triangles).

## Overflow and malformed-input behaviour

Structural worst case is 2,112 rim edges per patch (checkerboard breach —
derivation in terrain_rules §5). Emission is clamped to the declared
per-patch budget (provisional 512 quads) and degrades by merging collinear
spans; the clamp increments a counter and NEVER drops the rim span nearest
the camera (governor priority order).

## Counters and traces

`triangles_submitted` (shared geometry counter); rim-specific trace payload
(patch_id, edge index, merged-span length).

## Scalar reference function

`zref::forge::rim_plan` (reference/src/zterrain/terrain_core.cpp, deep-keel
wave) — enumerates one edge per SOLID cell side facing void/OUT per
32×32-cell page, applies the §5 frozen degrade order (minimum
contiguous-collinear merge — never bridging a notch — then greatest
endpoint 1/w, ties by scan order), and returns spans + merged + dropped
(dropped counts BODIES: a dropped span head takes its whole span). The
renderer's draw_heightfield emits exactly this plan — one rim law
(charter §29-6). Consumes the composed lattice (the tessellator's own
edge truth), not a second derivation.

## Directed tests

`tests/forge/forge_cliff_directed.cpp` (deep-keel wave): hand-counted
enumeration anchors (solid block 16, centre bite 24, checkerboard 4 per
solid cell), the 32×32 checkerboard structural clamp (2,048 → 512, the
TIGHT worst case — §5's 2,112 counts all adjacency edges and 64 of them
have void owners), near-camera priority survival, merge-under-pressure
with a partial-prefix expectation (the greedy sheds the MINIMUM),
default-no-merge control. Rim-edge LOD equality with the underside
remains a Phase-6 directed obligation (stitch sets do not exist at
Phase-3 reference scope).

## Randomized differential tests

`tests/forge/forge_cliff_random.cpp` (deep-keel wave): 300 random void
masks across page boundaries vs an independent enumeration oracle, the
per-page emission bound, the emitted-bodies + dropped == enumerated
identity, and plan purity. LOD-vector vertex-set closure (no cracks on
the silhouette) remains a Phase-6 obligation with the stitch sets.

## Formal properties

Emission bound: wall quads per patch ≤ clamp, always; no duplicate edge
emission (each rim edge emits at most once per frame).

## Synthesis / resource ceiling

Myriad + Primitive Forge group (charter §25: 9%).

## Integration capture cases

Phase-6/7 captures: giant edge-bite scar (rim + debris + sheet stamp in one
capture), breach punched through an island with both Duo cameras — the
signature-moment geometry (S3 §B1).

## Notes

Skirt depth from terrain LOD delta, not a tunable. Wall texture V spans
true local thickness — mirrored repeat turns the strata tile into geology
for free; every polygon this block emits is textured (owner requirement,
terrain_rules §5). Diagonal (45°) rim smoothing is explicitly NOT in v1 —
it must arrive as a paired amendment with the sim column query
(terrain_rules §5, last bullet).
