# ADDENDUM — World Identity: Terrain, Creatures, Giants

*Consolidating architect, RUN-20260816-0046. Inputs: FINDINGS-S1 (sacengine
terrain), FINDINGS-S2 (sacengine creatures), FINDINGS-S3 (Giants + identity
synthesis). Outputs landed in the repo at commits 91006c2 (terrain spec),
3be35d8 (terrain contracts), and the creature/ledger commit following.
Everything here is SPECIFICATION and CONTRACT — no RTL, no maturity above
SPECIFIED (charter §21/§4 honoured).*

---

## 1. THE CENTERPIECE — island rim topology (DECIDED)

**Decision (spec/terrain_rules.md §3): dual heightfield (top + bottom
surface) + per-cell substance state + a bake-time thickness-breach law.**
Each island column is one solid interval `[bottom, top]` or void.

### Why this and not the alternatives (all costed in the spec)

| Option | Cost | Verdict |
|---|---|---|
| Single heightfield + void mask (Sacrifice's own model) | baseline | Rejected as insufficient — cookie-cutter bites, faked underside, fails "more deformable than Sacrifice" exactly at the rim |
| **Dual heightfield + void + breach law** | **+2,178 B/patch (+11%)** | **CHOSEN** — bites, breaches, thin lips, shaped undersides/keels, true-thickness rim walls |
| 3D SDF | 128 KiB/patch → **131 MB resident = 8.2× the entire 16 MB terrain pool**; needs raymarch/isosurface (§26-refused compute) | Rejected on cost, twice over |
| Column runs (k=2 intervals) | 4× height bytes; stitch-case matrix squares; LOD collapse of a 2-run column IS a popping cave; ambiguous live "height" writes | Rejected — buys exactly what charter §11.7 Wounds already reserves |

### The load-bearing laws around the decision

- **Breach law:** only TERRAIN.BAKE (never a transient field) can turn a
  SOLID cell VOID_BREACHED — when composed top == bottom at all 4 corners.
  Permanent holes are therefore always born under an impact event whose FX
  masks the discrete transition; heals are the same law in reverse;
  authored void never becomes ground. Deterministic, capture-exact.
- **Lattice law (anti-drift):** field programs are evaluated ONLY at the
  33×33 lattice; render, sim height query, particle collision, velocity,
  normals and nav ALL interpolate the same lattice on the same
  triangulation (normative pseudocode, terrain_rules §4.3, one diagonal
  rule shared with the shipped reference renderer). sacengine's Erupt
  rebound drift (2.0 CPU vs 3.0 GLSL, S1) is the cautionary evidence; the
  bug class is now structurally impossible, not merely tested against.
- **Rim rendering (FORGE.CLIFF):** one textured wall quad per SOLID↔void
  lattice edge, top ends on the tessellator's stitched vertices (crack law),
  V spans TRUE local thickness (mirrored strata tile = geology for free).
  Structural worst case derived: 2,112 edges/patch (checkerboard); clamped
  at 512 quads with collinear span-merge. Every polygon textured — the
  owner's requirement is met by construction, walls and underside included.
- **Fall-through:** VOID/OUT columns return no ground; entities go
  ballistic and leave the world past the kill margin; particles pour
  through breaches with zero new code (PART.COLLIDE consumes the same
  query).
- **Undercut honesty:** single-interval columns give bitten-apple rims and
  overhanging lips but NOT caves/arches. Those stay charter §11.7 Wounds —
  and the void mask IS the Wound socket (mask cells void, parent a meshlet
  plug). The format needs nothing further, which is precisely why option
  (d) was refused.

### Cell pitch — frozen (was never ratified anywhere)

Per-island `pitch_log2 ∈ {−1..+2}` ({0.5, 1, 2, 4} m; shifts, not
divisions), canonical battlefield pitch 2.0 m.

## 2. Scale and texturing — the owner's two alarms, verified then answered

- **"16× a Sacrifice map"** — VERIFIED as cell count: 1,048,576 resident
  cells / 65,025 = 16.125. But stated honestly: as *area* at canonical 2 m
  pitch that is 4.19 km² vs the donor's 6.5 km² gross. The claim survives
  because **patch residency is sparse** — sky patches don't exist, while
  Sacrifice paid for its whole 256×256 square including void. A donor-scale
  island (~3.25 km² solid) costs ~793 of 1,024 resident patches at 5× the
  donor's deformation resolution and 25.6× its vertical step. **Conclusion
  confirmed: capability unbuilt, not unarchitected — no rearchitecture.**
- **"All polygons textured"** — the Sacrifice diet fits our silicon exactly
  (S1): one primary CLUT8 64×64 tile sample per fragment, per-cell (0,0)–
  (1,1) UV, MIRRORED repeat, zero blending; per-cell {matA, matB, weight}
  candidates picked by Mosaic; authored transition groups (MAPG heir)
  compose with Mosaic rather than competing. **Key packaging decision:**
  the LMAP-heir tint moved to per-VERTEX RGB565 (33×33/patch) riding the
  Gouraud path — so the ONE restricted aux sampler stays with the surface
  sheet and charter §26's "no second TMU" is never even tempted. Sized:
  4 resident tilesets = 5.3 MiB (11% of texture pool).

## 3. Memory — derived, fits with stated headroom

| Pool | Contents | Used |
|---|---|---|
| Terrain sheets + materials (16 MB) | sheets, materials, cell state, gameplay grid, vertex tint × 1,024 | 14.38 MiB |
| Terrain hot cache (12 MB) | height planes ×3 + headers (6.44) + LOD mips (1.45) + composed cache (0.53) + velocity (0.53) | 8.95 MiB |
| Textures (48 MB) | 4 tilesets | 5.3 MiB |

Streaming worst case 41 MB/s at 32 patches/frame — **provisional against
Phase-0 board truth (ZH-004), explicitly not frozen.**

## 4. Creatures (spec/creature_rules.md)

- **Format:** ring-cylinder parts are a TOOL format; hardware sees only
  meshlets + clip banks + poses. ≤32 bones, ≤2 influences, one CLUT8 page
  per part, attachment sockets. Cartridge kinds 8/9 added (semantic now,
  byte-freeze at Phase-12 entry — packer refuses to emit until then).
- **Animation:** 30 Hz keys, hard cuts over a 64-slot clip vocabulary
  (donor-proven; buys us out of a blend unit), keyframe event tags for
  damage/audio/impact sync.
- **Pose pipeline — a real decision with a derivation:** baking all clips
  to matrices at load (the donor's way) costs ×6 memory — 19.7 MB for ten
  types = 82% of the 24 MB mesh+anim pool. REJECTED. Chosen: clips stay
  compressed (8 B/bone/frame); new block **GEOM.POSE** (SPECIFIED, ZH-081)
  decodes (type, clip, frame) → matrix palettes into a shared 128-tuple
  (192 KiB) cache; type-grouped armies make hits the common case; ~49k
  multiplies/frame worst decode. Also satisfies the Phase-9 "no CPU
  per-limb submission" gate.
- **3→2 weight clamp — PRIOR CLAIM CORRECTED (loudly):** S2/S3 said the
  clamp error is "sub-quantum because weights are 6-bit." **Wrong.** The
  error is w₃·D — worst legal case 21/64 × joint offset ≈ 13 mm ≈ 2 px at
  2 m — three thousand quanta. Real distributions unmeasurable (donor art
  assets absent from the engine repo), so the spec gates at asset compile:
  exact per-vertex error over all clip frames, warn >1% / reject >3% of
  bound radius.
- **Alive-tricks (sim-side, near-free):** rotateOnGround (2 column-query
  taps + rate-limited tilt), bulk-then-pop gibs, tick-skip slow-motion,
  per-instance scale variety.

## 5. Giants

- **Data, not machinery** (donor law): scale + hitbox class + gait params +
  camera packages. Reparent verbs (grab/eat/throw/horn-larder/ride) are
  pointer swaps on the Loom — the biggest character-per-gate win (S3 #1),
  specified as the REPARENT node with epoch checks.
- **Weight = choreography + world response, never input latency** ("snappy
  controls, heavy consequences"). The impact chain is SIM-authored from
  clip event tags → SurfaceStamp + TerrainField + particle spawns + shake;
  hardware emits no gameplay events (charter §29-8 honoured).
- **Terrain-class giant — the seam is specified** (body_patch flag; Loom
  node binding constrained rigid+uniform-scale; column_query transforms
  into node space then runs the unchanged lattice law), and marked
  **PROTOTYPE-BEFORE-SILICON**: no RTL work may cite it before a software
  prototype capture exists. Mud-shepherds layer on this seam later (intent
  recorded, unscheduled).
- **Growth-as-scale:** mechanically near-free; the honest cost is
  split-screen fill — giant content tiers must reserve fragment tokens,
  declared at Phase 11, NOT invented now.

## 6. Rejected, with reasons

1. Giant as symmetric playable faction — Giants' own reviewers: magnificent
   to see, worst to play. Summonable/asymmetric-Duo instead.
2. No-culling/no-LOD creature rendering — donor survived on a desktop GPU;
   the mesh→micro→splat→glint ladder is mandatory here.
3. Per-face artist-authored LOD — MRMM rotted; sacengine ignores it.
   Compiler-generated screen-space-error collapse only.
4. Creature footprints on terrain — donor never consumed `foot` for
   terrain; possible later via the stamp lane, not spec'd, not budgeted.
5. SDF terrain, column-run terrain — §1 table.
6. Animation blend unit; per-draw 32-matrix uniform re-upload; second
   unrestricted TMU — all refused (charter §26 + donor budget hazards).

## 7. Ranked build order (world-identity increments on the phase plan)

1. **Island Patch v1 in ZRef** — `zref::terrain::column_query` + compose +
   breach law (extends Phase 6/7 work already scheduled; blocks everything
   below).
2. **TERRAIN.TESS both-surface reference + FORGE.CLIFF rim reference** with
   the crack/physics-equals-pixels tests (Phase 6).
3. **TERRAIN.BAKE incremental-scaling + breach/heal + SW.CPUCOLL mirror**
   (Phase 7) — the "more deformable than Sacrifice" proof point.
4. **Texturing lane** — tiles/Mosaic candidates/vertex tint (Phase 6,
   existing blocks, now with sized budgets).
5. **Clip banks + GEOM.POSE reference** (needs the quat qformats amendment
   first) + 3→2 clamp gate in SW.TOOLS.ASSET (Phase 9 prep).
6. **Loom vocabulary: SCALE + REPARENT + gait envelope** (Phase 9) + the
   sim impact chain in Form.
7. **rotateOnGround, bulk/gib, tick-skip** — sim-side, cheap, huge
   character (Phase 9/10).
8. **Growth tiers + giant camera packages** (Phase 11 presentation
   contracts).
9. **Terrain-class giant software prototype** (after 1–3; gate for any
   further giant-surface commitments).
10. **Palette zoning + transition-group authoring** (Phase 12 pipeline).

## 8. Costed vs not costed

**Costed with derivations:** rim options (bytes, pool fractions), patch
page (21,320 B; pool sums 14.38/8.95 MiB), tilesets (1.33 MiB each), rim
worst case (2,112 edges), pose pipeline (×6 bake penalty; 192 KiB cache;
49k mult/frame), weight-clamp error bound (w₃·D, px projections), streaming
(41 MB/s, provisional).

**Explicitly NOT costed (refused to invent numbers):** giant max-size fill
reservations (scenario-dependent → Phase 11 content tiers); GEOM.POSE
synthesis area (Phase 0 board truth pending, only the group ceiling is
stated); Wound plug geometry budgets; diagonal-rim triangle deltas;
board-true streaming headroom.

## 9. Blocked / not decided (and on what)

- GEOM.POSE REFERENCE_COMPLETE ← blocked on a **qformats amendment**
  (quantized-quat lane + decode rounding; QFMT_VERSION bump discipline).
- `ReparentTransform` ABI command ← proposed; lands via abi-gen
  regeneration by the ABI owner (not hand-edited into generated artifacts).
- Kind 8/9 byte layouts ← freeze with SW.TOOLS.ASSET at Phase-12 entry.
- Diagonal (45°) rim smoothing ← deferred; must arrive as a PAIRED
  sim-query + tessellator amendment or not at all (lattice law).
- Bottom-surface live deformation ← v1 bakes only; format rev if wanted.
- Wound plug format ← socket ready (void cells), format unscheduled.
- Sun-streak-on-water CLUT ramp ← **request to the sky/beams owner** (file
  untouchable this run): a distance-banded specular streak ramp indexed by
  a fixed-point dot toward the sun, marrying the ratified Noctis suns with
  water pooled in scars (S3 §A4.1). No format implications for terrain.

## 10. Prior-claim corrections found (welcome-news section)

1. **"3→2 weight clamp error is sub-quantum" (S2/S3) — WRONG**; corrected
   to the w₃·D bound with a compile-time gate (creature_rules §3).
2. **Cell metre-pitch was never ratified** — the reference renderer's
   "sub-metre by design" comment was a defect-note aside, not law. Now
   frozen ({0.5..4} m, canonical 2 m); the comment's demo case (0.6 m)
   remains legal content.
3. **The 16× scale answer needed honesty framing** — true in cells,
   misleading as area without the sparse-residency argument; both now
   stated with arithmetic in terrain_rules §1.4.
4. No previously-RATIFIED repo law was found wrong this run (the arbiter
   B-bound affair was already corrected by the prior wave).

## 11. Process notes

- devkitPro ctest reproduces the path-mangling failure exactly as the
  memory warns (all exe tests BAD_COMMAND); winlibs ctest at
  dsstuff/mingw64 used throughout. Baseline before edits: **48 passed,
  0 failed, 1 skipped (format_check)**; identical result required before
  each push.
- Ledger validated V1–V16 green at 88 blocks after GEOM.POSE; diagrams
  regenerated (V14).
