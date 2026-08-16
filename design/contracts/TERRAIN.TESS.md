# Contract — TERRAIN.TESS (Crack-safe tessellator)

> Ledger: `design/blocks.yml` · owner ZH-034 · phase 6 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` (world-identity wave). This block
> turns composed lattices into triangles for BOTH island surfaces — top and
> underside — with crack-safe stitching; rim walls belong to FORGE.CLIFF.

## Purpose and exclusions

Subpatch tessellation at crack-safe resolution with stitching and geomorph
between levels, for the top surface (composed lattice) and the underside
(bottom lattice, inverted winding), emitting triangles ONLY for SOLID cells
(terrain_rules §3.2/§5).

Exclusions: no rim walls (FORGE.CLIFF), no normals (TERRAIN.NORMALS), no
LOD decisions (TERRAIN.LOD decides; this block obeys).

## Clock and reset semantics

`gpu` domain, synchronous reset to IDLE; no persistent state beyond the
in-flight subpatch.

## Input and output packet layouts

- `patch_state` (TERRAIN.PATCH): composed-lattice + bottom-lattice handles,
  cell-state plane, per-subpatch dirty flags.
- `lod_target` (via TERRAIN.LOD): per-subpatch grid resolution + geomorph
  factor + the stitched edge-level vector (4 neighbour levels).
- Output `terrain_mesh`: vertex/triangle packets in the common geometry
  format (charter law: weird units feed common endpoints), tagged
  top/underside, with source IDs.

## Backpressure rules

ready/valid; a stalled consumer stalls the tessellator without state loss.

## Memory ownership

Read-only on lattices and cell state. Writes nothing to VRAM; emits stream
packets only.

## Q formats and rounding

Vertex positions: lattice values verbatim (fx16) — the tessellator never
re-rounds heights; interpolated geomorph vertices use qformats §3 fx_mul
single-rounding on the SAME §4.3 triangulation formulas. Cell triangulation
diagonal law: terrain_rules §4.3 (i00–i11 diagonal, ties to triangle A) —
identical to the sim column query by construction.

## Latency (fixed or variable)

Variable; bounded per subpatch by the max grid resolution (8×8 cells → ≤
2×8×8 = 128 top triangles + 128 underside per subpatch at full resolution).

## Target throughput

1 emitted vertex per clock.

## Overflow and malformed-input behaviour

A lod_target above the legal resolution set is clamped + counted; a
subpatch whose cells are all void emits nothing (legal, common — sparse
islands); packet-format violations are impossible by construction upstream.

## Counters and traces

`terrain_triangles_emitted` (split top/underside in the trace payload).

## Scalar reference function

`zref::TerrainTess` — shares the §4.3 triangulation helpers with
`zref::terrain::column_query`; one implementation of the cell split exists
in the codebase (charter §29-6).

## Directed tests

`tests/terrain/terrain_tess_directed.cpp`: every stitch pattern pair
(neighbour level deltas), geomorph endpoints (factor 0/1 = exact levels),
void-cell emission holes, underside winding, rim-edge LOD equality
constraint (terrain_rules §5: underside LOD = top LOD along rim boundary
edges).

## Randomized differential tests

`tests/terrain/terrain_tess_random.cpp`: random lattices + void masks + LOD
vectors vs zref; assert no T-junctions (vertex-set closure on shared edges)
and `physics_equals_pixels` sampling at random interior points.

## Formal properties

Crack-safety as a stitch invariant: for any two adjacent subpatches at
legal level pairs, the shared-edge vertex sets are identical (formal
candidate, lands with the RTL per charter §20.4).

## Synthesis / resource ceiling

`geometry_mantle` group (charter §25, 20% ceiling).

## Integration capture cases

Phase-6 gate captures: Duo island from two cameras, all stitch patterns
crack-free (charter §23); breach hole + rim (with FORGE.CLIFF) capture.

## Notes

Crack-safety is a formal candidate (stitch invariants). The tessellator and
the sim column query share one triangulation law — that shared law, not
testing alone, is what makes physics equal pixels (terrain_rules §4).
