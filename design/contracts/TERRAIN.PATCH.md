# Contract — TERRAIN.PATCH (Terrain patch state engine)

> Ledger: `design/blocks.yml` · owner ZH-033 · phase 6 · maturity SPECIFIED
>
> Format law: `spec/terrain_rules.md` (Island Patch v1 — filled by the
> world-identity wave, RUN-20260816-0046). This contract is the Mantle entry
> point: it owns patch state residency, the bounded live-field intake, and
> the once-per-frame composed-height cache every other consumer reads.

## Purpose and exclusions

Own patch state layers (terrain_rules §2: header + layers A–H) and the
bounded field-list intake with bake/compose/reject-on-overflow (charter
§11.4). Produce the composed-height and velocity lattices (terrain_rules
§4.2) once per frame per touched patch.

Exclusions: does NOT tessellate (TERRAIN.TESS), does NOT write scars or
breach cells (TERRAIN.BAKE only), does NOT stamp sheets (SURFACE.STAMP),
does NOT evaluate field programs (FIELD.SEQ.EARTH does; this block places
results into lattices).

## Clock and reset semantics

`gpu` clock domain. Synchronous active-high reset returns the block to
IDLE with an empty field intake and an invalidated composed cache; patch
pages in VRAM are unaffected by reset (they are re-validated by page CRC at
next residency touch).

## Input and output packet layouts

- `dispatch` (from CMD.SCHEDULER): TerrainField records (commands.zidl
  0x0200) binned to patches by footprint intersection.
- `field_results` (from FIELD.SEQ.EARTH): per-lattice-vertex out-lane
  records {vertex_index u16, height fx16, velocity fx16, material u32,
  nav_cost fx16} for the patch under evaluation.
- `baked_scars` (from TERRAIN.BAKE): scar-plane (layer B) and cell-state
  (layer D) update notifications with dirty rectangles.
- Output `patch_state`: composed-lattice handles {patch_id, live_top_ptr,
  velocity_ptr, dirty_flags, lod_mip_ptrs} consumed by TERRAIN.TESS/LOD/
  VELOCITY and PART.COLLIDE.
- Output `subpatch_requests`: 8×8-cell subpatch work items toward
  TERRAIN.TESS.

Exact packet field widths are frozen at REFERENCE_COMPLETE together with
the zref reference; the lattice element formats are already frozen
(height16 storage, fx16 live math — terrain_rules §3.4, qformats §2/§9).

## Backpressure rules

ready/valid on all ports. The field intake is bounded per patch (charter
§11.4): **MAX_PATCH_FIELDS = 16** live programs per patch per frame, frozen
with derivation in terrain_rules §9.1. ~~When a patch's per-frame field list
would overflow, the block REJECTS the lowest-priority cosmetic fields~~
(superseded 2026-08-16 — the hardware has no priority notion): on overflow
the block rejects the incoming record — the first 16 in command order win,
counted in `programs_rejected` plus a trace event, nothing listed is ever
evicted, and the frame never stalls. Priority ordering is the SIM's job
above the seam (it emits droppable cosmetics last per patch and applies the
§11.4 bake/compose/degrade valves before emission). Reject, never silently
drop, never overrun — and rejects are capture-replay exact because they are
a pure function of the command stream.

## Memory ownership

Reads layers A/C (authored heights) and B/D (scar/cell-state, owned by
TERRAIN.BAKE) read-only. Owns and writes ONLY the composed-height and
velocity lattices in the terrain hot cache (terrain_rules §7/§8). All VRAM
access via MEM.GUARD regions; the composed cache region is this block's
exclusive write grant.

## Q formats and rounding

terrain_rules §3.4 verbatim: compose in fx16 with qformats §3 saturating
`fx_add`, clamp `max(·, bottom)`; height16 ↔ fx16 conversions per qformats
§2/§9 (exact `<<8` up, round-half-up bake-back — bake-back lives in
TERRAIN.BAKE, not here). No other rounding exists in this block.

## Latency (fixed or variable)

Variable: proportional to touched patches × live fields. Bounded by the
per-patch field ceiling (MAX_PATCH_FIELDS = 16, terrain_rules §9.1) and the
1,024-patch residency; the frame scheduler sees it as an ordinary engine
with a deadline. Note the §9.1 cost-coupling honesty line: a worst-legal
patch is 16 × 1,089 × 32 ≈ 557k field instructions — the intake bound is
not a frame-affordability certificate; the frame-level budget is NOT COSTED
until FIELD.SEQ.EARTH's throughput is pinned.

## Target throughput

1 patch-layer update per clock (lattice vertex compose per clock steady
state).

## Overflow and malformed-input behaviour

- Field-list overflow: reject lowest priority + count (`programs_rejected`).
- Footprint outside the patch envelope: clipped to envelope (deterministic).
- fx16 saturation: recorded via SatLedger-mirroring counters (qformats §5).
- Malformed dispatch records cannot reach this block (CMD.DECODER validates);
  a bad patch handle is a safe no-op + error counter.

## Counters and traces

`terrain_samples_evaluated` (lattice vertices composed), plus
`programs_rejected` shared with the field cache lane. Trace point: composed
lattice writes (patch_id, vertex, value) selectable into DEBUG.TRACE.

## Scalar reference function

`zref::TerrainPatch` — composes lattices by calling the ONE zfield
interpreter at lattice vertices only (terrain_rules §4.1 lattice law) and
the §3.4 clamp chain. `zref::terrain::column_query` (terrain_rules §4.3) is
the shared query consumed by SW.CPUCOLL/PART.COLLIDE tests.

## Directed tests

`tests/terrain/terrain_patch_directed.cpp`: compose identities (zero fields
= base+scar clamp), clamp-at-bottom cases, field-order determinism, breach-
adjacent lattice sharing (void cells' corners still composed), overflow
reject order.

## Randomized differential tests

`tests/terrain/terrain_patch_random.cpp`: random patches + field lists vs
`zref::TerrainPatch`; includes the `physics_equals_pixels` obligation
(terrain_rules §10.1) once TERRAIN.TESS exists.

## Formal properties

Bounded intake: the per-patch field list never exceeds MAX_PATCH_FIELDS
(16, terrain_rules §9.1); the composed-cache write pointer never leaves the
granted region (rides `mem_guard_no_escape` at integration).

## Synthesis / resource ceiling

Inside the `geometry_mantle` group budget (charter §25: Geometry + Measure
+ Mantle = 20%). No per-block absolute numbers before Phase 0 board truth.

## Integration capture cases

Wound Lab Phase-6/7 captures: static island (no fields), one crater field,
two opposing waves (charter §23 Phase-7 gate), breach-birth capture
(terrain_rules §3.5) — all replay-exact by tile CRC.

## Notes

Overflow policy is reject, never silently drop (§11.4). The composed cache
is produced ONCE per frame and shared by both Duo views (charter §11.5) and
by the sim mirror — the anti-drift lattice law (terrain_rules §4) is the
load-bearing design fact of this block.
