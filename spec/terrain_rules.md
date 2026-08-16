# Zhaozhou Terrain Rules — Island Patch Format v1

**Status:** new spec, world-identity wave (run RUN-20260816-0046, consolidating
recons S1/S2/S3). This file is the single law for the Mantle terrain field
format: patch layers, the island **rim topology decision** (bites, breaches,
undersides), the lattice evaluation law that keeps sim and pixels identical,
the texturing lane, and the derived memory/bandwidth budgets. Where this file
and any other prose disagree about terrain, this file wins; numeric formats
defer to `spec/qformats.md` (the numeric law), command wire layouts to
`spec/commands.zidl`, and container bytes to `spec/cartridge.md`.

Charter anchors: §11 (Mantle), §12 (Scar Scribe), §15 (one primary TMU +
restricted aux — §26 forbids a second unrestricted TMU), §29-6 (never
implement semantics twice), §29-7 (no host floats in deterministic paths).

Donor evidence: sacengine recon S1 (Sacrifice terrain: 255×255 cells at
10 u/cell, zero-blend tile texturing, two-tier deformation, and a genuine
CPU/GPU drift bug where Erupt's rebound height was 2.0 in sim and 3.0 in the
shader — the proof that dual implementations of field semantics rot).

---

## 1. World scale — derived, not asserted

### 1.1 The donor yardstick

A Sacrifice map is one monolithic 256×256-vertex heightfield: **255×255 =
65,025 cells at 10 units/cell ≈ 2,550 m per side ≈ 6.5 km² gross** (S1 §1;
unit ≈ metre by creature scale). A large fraction of that square is void —
islands float in sky — and Sacrifice pays storage and draw for the full
square regardless.

### 1.2 The Zhaozhou frame

Already ratified elsewhere and reused here unchanged:

- patch = **32×32 cells** on a **33×33 vertex lattice** (charter §11.1);
- resident set = **1,024 patches** (charter §12 sheet arithmetic);
- heights stored as `height16` = S 1.7.8 metres, ±128 m at ~3.9 mm step
  (qformats §9), live math in fx16.

Resident cells = 1,024 × 32 × 32 = **1,048,576 = 16.1× Sacrifice's 65,025**
(1,048,576 / 65,025 = 16.125 — verified).

### 1.3 Cell pitch (NEW, frozen here)

The metre size of a cell was not previously frozen anywhere (a code comment
in `reference/src/zrender/terrain.cpp` says "sub-metre by design" inside a
defect note; that was never law). Frozen now:

- **Pitch is per-island**, restricted to **{0.5, 1.0, 2.0, 4.0} m**, encoded
  as `pitch_log2 ∈ {−1, 0, +1, +2}`. Powers of two make world→cell lookup a
  shift and keep every lattice x/z exactly representable in fx16 — no
  division, no rounding, anywhere in the addressing path.
- **Canonical battlefield pitch = 2.0 m** (patch = 64 m per side).

### 1.4 What the numbers mean (the "terrain too small" alarm, answered)

At canonical 2.0 m pitch:

| Measure | Sacrifice | Zhaozhou resident set | Ratio |
|---|---|---|---|
| Deformation lattice pitch | 10 m | 2 m | **5× finer** |
| Vertical step | 0.1 m (ushort×0.1) | 3.9 mm (height16) | **25.6× finer** |
| Resident cells | 65,025 (incl. void) | 1,048,576 | **16.1×** |
| Resident ground area | ≤6.5 km² gross, void included | 4.19 km² of *registered* patches | comparable solid ground |
| World beyond residency | none (all resident, always) | streamable per patch | open |

The load-bearing structural point: **patch residency is sparse.** A patch
that is entirely sky simply does not exist — no page, no sheet, no draw.
Sacrifice pays for its void; we do not. A Sacrifice-scale island whose solid
ground is ~3.25 km² (a generous 50% of the donor square) costs
3.25 km² / (64 m)² ≈ **793 patches — inside the 1,024 residency with 231
patches spare**, at 5× the donor's deformation resolution. At 4.0 m pitch the
same residency spans 16.8 km². Conclusion (confirming the prior ruling):
**no rearchitecture was needed; the capability was unbuilt, not
unarchitected.**

### 1.5 Island table

Islands are the unit of world composition. Each island carries:

- `island_id` (u32), world origin `world3` (fx16 x/z, **fx16 y datum** — patch
  heights are relative to the island datum, so islands live at any altitude
  inside the fx16 ±32 km world while height16 stays ±128 m of local relief);
- `pitch_log2` (i8), patch-grid extent (u16 × u16), tileset id (u32);
- a sparse patch directory: (ix, iz) → patch page handle. Absent entry =
  open sky (`OUT`).

---

## 2. Patch state layers — Island Patch v1

Per patch (all little-endian; offsets within the VRAM-resident page):

| # | Layer | Extent | Element | Bytes | Notes |
|---|---|---|---:|---:|---|
| — | Header | — | — | 64 | §2.1 |
| A | Top base height | 33×33 | height16 | 2,178 | authored, immutable at runtime |
| B | Top scar delta | 33×33 | height16 | 2,178 | persistent deformation, bake-written (TERRAIN.BAKE) |
| C | **Bottom height** | 33×33 | height16 | 2,178 | the island underside — NEW layer, §3 |
| D | Cell state | 32×32 | u8 | 1,024 | substance + flags, §3.3 |
| E | Base material | 32×32 | {matA u8, matB u8, weight unit8} | 3,072 | Mosaic candidates, §6 |
| F | Surface sheet | 64×64 | {tag u8, strength u8} | 8,192 | charter §12, unchanged |
| G | Gameplay grid | 8×8 | {heat u8, wet u8, corrupt u8, hazard u8} | 256 | 4×4 cells per gameplay cell |
| H | Vertex tint | 33×33 | RGB565 | 2,178 | LMAP heir, per-VERTEX (§6.4) |
| | **Total** | | | **21,320** | page stride 21,376 B (334 × 64-B bursts, 56 B pad) |

Layers derived at load/bake, not streamed: coarse height mips per surface
(17×17 + 9×9 = 740 B ×2 surfaces = 1,480 B/patch) for TERRAIN.LOD, and the
per-frame composed-height cache (§4.2).

### 2.1 Header (64 B)

```
+0   u16 format_version   = 1
+2   i8  pitch_log2       −1..+2 (must match the island table)
+3   u8  flags            bit0 = body_patch (giant-body seam, creature_rules §7)
                          bits 1-7 reserved 0
+4   u32 island_id
+8   i16 patch_ix, patch_iz     patch coords in the island grid
+12  u32 tileset_id
+16  rectfx envelope      x0,z0,x1,z1 (fx16 world, island-datum-relative; must
                          equal origin + coords × 32 × pitch exactly — the
                          packer asserts it; redundancy is a corruption check)
+32  u32 page_crc32c      over bytes [64, 21320)
+36  u8  rsv[28]          must be 0
```

## 3. THE RIM TOPOLOGY DECISION

**Decision: dual heightfield (top + bottom surfaces) + per-cell substance
state + a bake-time thickness-breach law.** Each column of an island is one
solid interval `[bottom, top]` or void. This is the format for edge-bites,
breaches, thin overhanging lips, and shaped undersides.

### 3.1 Options considered, costed

**(a) Single heightfield + void mask (the Sacrifice model).** 2,178 + 1,024 B.
Underside must be faked (Sacrifice mirrors the whole top surface at −50 with
a fixed-depth skirt). Bites are cookie-cutter holes with constant-depth walls;
no thickness variation, no shaped keel, no thin lips. Fails the stated
product requirement ("more deformable than Sacrifice") at exactly the rim,
which is where floating islands live or die visually. **Rejected as
insufficient — kept as the degenerate case (a bottom plane is legal
content).**

**(b) Dual heightfield + void mask — CHOSEN.** Adds one 33×33 height16 plane:
**+2,178 B/patch = +11%** over (a)'s equivalent page. Expressible: edge-bites
(columns → void), breaches punched through (void with solid neighbours on all
sides), tapered undersides and keels, thin protruding lips (a column whose
bottom is high — a slab), rim walls of *true local thickness*. Not
expressible: two solid intervals in one column (true caves/arches/tunnels).

**(c) Signed-distance volume.** To cover the ±128 m local range at even 2 m
voxels: 32×32×128 × 1 B = 128 KiB/patch → **131 MB resident — 8.2× the
entire 16 MB terrain pool**; a 4 m-voxel economy cut still needs 16 MB for
geometry alone and cannot express a 2 m bite it was bought to express.
Rendering needs raymarch or isosurface extraction — a general compute path
charter §26 refuses. **Rejected on cost, twice over.**

**(d) Column runs (k solid intervals per column).** At k = 2: 8 B/vertex →
8,712 B/patch (4× the chosen height planes) plus run-count planes. Real costs
are structural, not bytes: the height query grows an interval-select branch;
crack-safe stitching must reconcile neighbours with different run counts
(the stitch case matrix squares); LOD collapse of a 2-run column to a 1-run
coarse level *is* a popping cave; live "height" writes become ambiguous
(which interval's top?). All of that buys exactly what charter §11.7 already
reserves for **Wounds** — bounded volumetric hero features. **Rejected;
see §3.6 for how Wounds socket into the chosen format instead.**

### 3.2 Column law

Every column (cell) of a registered patch is exactly one of:

- `SOLID` — one material interval `[bottom, top]`, `top ≥ bottom`;
- `VOID_AUTHORED` — never ground (the authored island outline);
- `VOID_BREACHED` — ground that has been destroyed at runtime;
- absent patch ⇒ `OUT` (open sky; behaves as void for all queries and rims).

Lattice values (heights, tint) exist at all 33×33 vertices regardless of the
substance of adjacent cells — void cells' corner vertices are shared with
solid neighbours and are what rim walls hang from.

### 3.3 Cell state byte (layer D)

```
bits 1:0  substance   0 = SOLID, 1 = VOID_AUTHORED, 2 = VOID_BREACHED, 3 = reserved
bit  2    no_bake     protected cell: bakes clamp so the cell can never breach
                      (spawn platforms, altar plinths)
bits 7:3  reserved 0
```

### 3.4 Height composition and the breach law

All compose math is fx16 with qformats §3 saturating ops; height16 ↔ fx16
conversions per qformats §2/§9.

```
base, scar, bottom : height16 lattice planes (A, B, C)
compose_top[v] = max( fx(base[v]) + fx(scar[v]),  fx(bottom[v]) )      // clamp at the underside
live_top[v]    = max( compose_top[v] + Σ field height lanes (command order,
                                          fx_add chain),  fx(bottom[v]) )
```

- **Live fields apply to the top surface only** and only ever *between* the
  clamped bounds. The bottom surface changes exclusively at bake time.
- **Breach law (evaluated only by TERRAIN.BAKE, discrete, deterministic):**
  after a bake writes scar values, a `SOLID` cell with `no_bake = 0` becomes
  `VOID_BREACHED` iff `compose_top[v] == fx(bottom[v])` (exact equality after
  the clamp) **at all four of its corner vertices**. A later bake that raises
  `compose_top` above `bottom` at any corner returns the cell to `SOLID`
  (heal). `VOID_AUTHORED` never becomes ground.
- Consequences, by construction: transient waves can never punch a permanent
  hole (only bakes breach); a breach is always born at a bake event — the
  moment the impact FX (debris, dust, shake) masks the discrete cell
  transition; breaches and heals are capture-replay exact.

### 3.5 What a breach IS, end to end

- **Sim:** the column query (§4) returns `VOID` → no ground. Entities above
  it are ballistic; an entity falling below `island_datum + min(bottom) −
  KILL_MARGIN` (game rule, not format) is removed — things genuinely fall
  out of the world, which no donor could offer.
- **Render:** TERRAIN.TESS emits no top/bottom triangles for the cell;
  FORGE.CLIFF emits rim walls around it (§5); through the hole: sky (the
  under-sky is the sky lane's business, `spec/sky_and_beams.md` — not
  restated here).
- **Particles:** PART.COLLIDE's heightfield test consumes the same column
  query; a particle over a void column simply never collides — debris pours
  through breaches and off rims for free.
- **Nav:** breach/heal marks the owning gameplay cells dirty; the sim-side
  nav grid re-reads them (SW.CPUCOLL). Sacrifice never updated nav after
  deformation (S1 §4) — we do, and it costs a dirty-bit, not a system.

### 3.6 Undercuts, and the honest limit

Expressible: a rim whose underside curves up INTO the island (bottom rising
toward top near the edge — the bitten-apple profile), overhanging thin lips
(high-bottom slab columns), keels, wedges. Not expressible: a second solid
interval in one column — true caves, arches, through-tunnels seen from the
side. Those remain charter §11.7 **Wounds**: bounded volumetric hero
features, realised as authored/procedural meshlet plugs socketed into
void cells (mask the cells `VOID_AUTHORED`/`VOID_BREACHED`, parent the plug
meshlets to the island's transform). **The void mask is the Wound socket;
the patch format needs nothing further now, and that is why option (d) was
refused.** The Wound plug format itself is NOT decided here (blocks nothing
until a Wound is scheduled; it is a Phase-11+ hero feature).

## 4. The lattice law — one evaluation, every consumer

This is the anti-drift law. Sacrifice evaluated its deformation twice — once
in the sim, once re-implemented in GLSL — and the two copies drifted (Erupt
rebound 2.0 vs 3.0, S1 §4). Charter §29-6 forbids that road.

### 4.1 The law

1. Earth field programs are evaluated **only at lattice vertices** (33×33
   per patch, top surface), by the one zfield interpreter
   (`spec/form/field-ir.md` §1.1), through the §3.4 composition.
2. **Every consumer** — tessellation/render, sim height query, particle
   collision, velocity, normals, nav — reads the **same composed lattice
   values** and interpolates them on the **same triangulation** (§4.3).
3. Nothing, anywhere, evaluates a field program at a non-lattice point.
   There is no "shader copy". There is no second implementation to drift.

### 4.2 Composed-height cache

The composed `live_top` lattice for every patch touched by a live field (and
every visible patch needing it) is produced **once per frame** into the
composed-height cache and consumed by both views and by the sim mirror
(charter §11.5). Budget: 256 live/visible patches × 2,178 B = 545 KiB, plus
the velocity lattice (height16-scaled, 2 B/vertex) 545 KiB — inside the
terrain hot-cache pool (§8).

### 4.3 Triangulation and point query (normative pseudocode)

Cell (i, j), corner indices i00=(i,j), i10=(i+1,j), i01=(i,j+1),
i11=(i+1,j+1), fixed diagonal i00–i11 (matches the shipped reference
`draw_heightfield` emit order: (i00,i11,i10), (i00,i01,i11)):

```
column_query(island, wx, wz) -> {class, top, bottom, velocity, matA, matB, weight, sheet}
  (ix, iz, patch) = sparse directory lookup            // absent -> OUT
  u16 fracs: cx = (wx - env_x0) >> pitch_log2 ...      // shift, no division
  cell = (floor cx, floor cz); u = frac(cx), v = frac(cz)   // fx16 fractions
  if substance(cell) != SOLID -> {VOID}
  pick triangle: u >= v ? A(i00,i10,i11) : B(i00,i01,i11)   // ties to A
  A: h = h00 + fx_mul(u, h10-h00) + fx_mul(v, h11-h10)      // 2 MADs, qformats §3
  B: h = h00 + fx_mul(u, h11-h01) + fx_mul(v, h01-h00)
  top    = interp(live_top lattice);  bottom = interp(bottom lattice)
  velocity = interp(velocity lattice)
  matA/matB/weight from layer E at cell; sheet from layer F (§6)
```

Corner identities (exact): A(0,0)=h00, A(1,0)=h10, A(1,1)=h11; B(0,0)=h00,
B(0,1)=h01, B(1,1)=h11; both give h00+fx_mul(u, h11−h00) on the diagonal —
the seam is single-valued. The reference symbol is
`zref::terrain::column_query`; SW.CPUCOLL, PART.COLLIDE and TERRAIN.TESS all
cite and consume it. **Differential test obligation:** for random (wx, wz)
the sim query result must equal the height of the rendered triangle at that
point exactly (the "physics equals pixels" test, `tests/terrain/`).

### 4.4 Normals and velocity

Normals are derived from the composed lattice by finite differences at
tessellation time, dirty patches only (TERRAIN.NORMALS) — Sacrifice kept
pre-crater normals forever (S1 §4); we recompute exactly where the ground
moved. Velocity is the Earth velocity out-lane accumulated at lattice
vertices (TERRAIN.VELOCITY), interpolated by the same §4.3 rule.

## 5. Rim geometry (FORGE.CLIFF law)

- **Rim edge** = a lattice edge between a SOLID cell and a void/OUT
  neighbour (4-neighbourhood, axis-aligned in v1).
- **Wall** = one quad per rim edge: top ends at the two composed-top edge
  vertices **exactly as emitted by the tessellator's stitched edge set** at
  the owning subpatch's LOD level; bottom ends at the bottom-lattice values
  of the same two vertices. Crack law: along any rim boundary the underside
  LOD must equal the top LOD on that edge (a stitch constraint, formal
  candidate).
- **Underside** = the bottom lattice triangulated by the same §4.3 rule with
  inverted winding, emitted for SOLID cells; LOD may be coarser than top
  except along rim boundaries (above).
- **Texturing:** walls sample a strata tile (tileset-reserved id range) with
  U = accumulated rim length / STRATA_M and V = (top − y) / STRATA_M,
  mirrored repeat (STRATA_M default 8 m, asset-tunable); the underside
  samples an underside tile with planar world UV / 8 m. **Every emitted
  polygon is textured** — top (tiles), walls (strata), underside — meeting
  the stated product requirement with zero new sampler hardware.
- **Bounds (derived):** a 32×32 patch has 2×32×31 interior + 4×32 border =
  **2,112 cell-adjacency edges**; that is the structural worst case
  (checkerboard breach). Typical convex rim through a patch ≈ 32–64 edges.
  FORGE.CLIFF must clamp emission to a declared per-patch budget
  (provisional 512 quads) and degrade by merging collinear spans — the
  governor sees wall quads as ordinary triangles_submitted.
- Diagonal (45°) rim smoothing at corner-void configurations is **not in
  v1** — deliberately: the sim column query and the tessellator must adopt
  any diagonal rule *together* (lattice law §4.1), so it ships as a later
  paired amendment or not at all. Axis rims agree with the column query by
  construction today.

## 6. Texturing lane (the Sacrifice diet, sized)

One primary TMU sample per terrain fragment. No inter-material blending,
ever (S1 §3: the donor shipped a AAA look with zero blending).

1. **Tile library:** 256 tiles per tileset, 64×64 CLUT8 + full mip chain =
   5,461 B/tile → **1.33 MiB per tileset** (+512 B RGB565 palette). Four
   resident tilesets = 5.3 MiB = 11% of the 48 MB texture pool.
2. **Per-cell candidates (layer E):** matA, matB (tile ids) + unit8 weight.
   TEXTURE.MOSAIC picks A or B per texel with the stable world-space pattern
   (charter §12) — one id wins, one sample happens. Cell UV = (0,0)–(1,1)
   mirrored repeat, the donor's seam-free trick, native to our TMU.
3. **Transition groups (MAPG heir):** per tileset ≤16 authored groups
   {member_count u8, detail_id u8, members u8[14]} = 256 B; authored
   transition tiles and Mosaic composition are complementary (S1 §3) —
   Mosaic may restrict its pick to a group's members for painterly borders.
4. **Vertex tint (layer H, LMAP heir):** 33×33 RGB565 per patch modulating
   the lit vertex colour — per-vertex like the donor's LMAP (1 texel/vertex),
   so it rides the Gouraud path and **costs no sampler**. Bakes AO, colour
   variation, faction stain washes.
5. **Surface sheet (layer F)** stays the restricted-aux sample (charter §15
   aux list) for tag/strength effects — the aux budget holds: ONE aux
   consumer on terrain fragments, because tint moved to vertices.
6. **Strata/underside tiles:** tileset ids 240–255 reserved by convention
   (packer-enforced), so cliffs match their island's palette.
7. **Palette zoning (identity rule):** island tilesets own the loud
   Sacrifice-lineage CLUTs; sky/void assets own the disciplined Noctis
   CLUTs. The contrast is the look. CLUT id ranges are partitioned by the
   asset pipeline (Phase 12), not by hardware.

## 7. Streaming, residency, ownership

- Patch pages stream whole (21,376 B stride, 334 bursts) HPS→VRAM through
  the ordinary upload path; a page is immutable in flight (frame-ownership
  law, charter §7.4). Worst-case sustained streaming at 32 patches/frame =
  684 KB/frame ≈ **41 MB/s** — provisional against the Phase-0 bandwidth
  matrix (board truth pending; re-check at ZH-004, do not freeze).
- VRAM ownership: layers A/C/E/H read-only to fabric; B (scar) written only
  by TERRAIN.BAKE; D written only by TERRAIN.BAKE (breach/heal); F written
  only by SURFACE.STAMP; the composed cache written only by TERRAIN.PATCH.
  MEM.GUARD regions extend accordingly at Phase 6.
- The sim (SW.CPUCOLL) owns the canonical mirror of B/D and the nav grid;
  the FPGA bake and the sim bake are the same deterministic function
  (charter: CPU owns canonical scars).

## 8. Memory budget (derived)

| Pool (charter §7.1) | Contents | Bytes |
|---|---|---:|
| Terrain sheets + material maps (16 MB) | F+E+D+G+H × 1,024 = 14,722 B × 1,024 | **14.38 MiB** ✓ |
| Terrain hot cache (12 MB) | heights A+B+C+hdr 6,598 B × 1,024 = 6.44 MiB; LOD mips 1.45 MiB; composed cache 0.53 MiB; velocity 0.53 MiB | **8.95 MiB** ✓ |
| Textures (48 MB) | 4 tilesets ≈ 5.3 MiB | ✓ (11%) |

Headroom is real, not decorative: sheets pool retains 1.6 MiB, hot cache
retains 3 MiB for Phase-7+ growth (larger live sets, deeper mips).

## 9. Field/stamp interplay (unchanged contracts, restated)

- Transient analytic waves (Erupt/Quake grammar, S1 §4) are ordinary earth
  programs with hard finite footprints — raised-cosine + polynomial
  envelopes need one sin LUT (qformats §7.1) and fit the earth ceiling (32
  ops, field-ir §"ceilings").
- Persistent stamps use incremental scaling (`applyDMapDelta(from,to)` heir):
  the bake applies `(to − from) × stencil` so an interrupted cast un-applies
  cleanly and permanence decays to a residual fraction — one stamp record,
  no per-frame rewrites. Stamp stencils are small integer assets (the donor's
  33×33 ubyte volcano stencil is the existence proof).
- Bounded lists per patch with bake/compose/reject on overflow: charter
  §11.4 verbatim; TERRAIN.PATCH enforces.

## 10. Test plan (obligations for Phase 6/7 owners)

1. `physics_equals_pixels`: random columns; `zref::terrain::column_query` ==
   rendered triangle height, exact (§4.3).
2. Breach determinism: bake sequences → identical D-layer transitions in
   ZRef/ZEmu and capture replay; heal round-trip; `no_bake` clamp.
3. Rim crack-freedom: all subpatch LOD pairs × rim configurations — no
   T-junction gaps between top, wall, underside (formal candidate with the
   TERRAIN.TESS stitch invariants).
4. Checkerboard worst case: 2,112-edge patch stays inside the FORGE.CLIFF
   emission clamp and degrades by span-merge, never by dropping the rim
   nearest the camera.
5. Streaming: page CRC verified before first use; torn-page impossibility
   rides the frame-ownership law.
6. Budget assertions: §8 sums recomputed by the ledger/report tooling once
   Phase 6 allocators exist.

## 11. Explicitly not decided here (and what it blocks)

- **Diagonal rim smoothing** — deferred (paired sim+tess amendment); blocks
  nothing, costs rim beauty at corners.
- **Wound plug format** (§3.6) — deferred to its phase; the socket is ready.
- **Bottom-surface live deformation** — v1 bakes only; a rippling underside
  would need a format rev (new live lane), noted as a possible v2.
- **STRATA_M, wall emission budget (512), 4-tileset residency** — tunables
  with stated defaults, re-ratified at Phase 6 with rendering evidence.
- **Streaming rate vs board truth** — §7 number is provisional until ZH-004.
- **Kill-plane margin and soul-return rules** on fall-through — game design,
  not format.
