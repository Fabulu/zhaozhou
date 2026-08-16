# RECON S1 — sacengine terrain system

*Fable recon agent, 2026-08-16. Sources: `C:\programmieren\sacengine\source\{sacmap.d, maps.d, levl.d, envi.d, state.d, renderer.d, dagonBackend.d}` + tg-2/dagon @6c41ec1 (`terrain2.d`, `shadow.d`). sacengine repo left clean. Persisted by orchestrator.*

## 1. World scale — THE MEASURED ANSWER

**A Sacrifice map is a 256×256-vertex heightfield = 255×255 cells, cell size 10.0 world units, one monolithic block. No tiles/patches/chunks at all.**

- Cell size 10.0 (`sacmap.d:181,184`). HMAP carries width/height (`maps.d:14-17`) but everything else hardcodes 256: TMAP `512-byte header + 256×256 bytes` (`maps.d:45-47`), LMAP `4*256*256` RGBA (`maps.d:83`), permanent deformation `float[256][256]` (`state.d:6787`), GPU displacement 256×256 R32F (`terrain2.d:527`), UVs ÷256.0f (`sacmap.d:474`).
- Height: ushort/vertex, top bit = void flag, `heights=elevation*0.1f` (`maps.d:23-27`) → 0.1-unit steps.
- **Total extent: 2560×2560 units ≈ 2.5 km × 2.5 km.** Creatures ~1–3 units tall; a unit ≈ a metre (`directWalkDistance=5.0f`, `state.d:565`).
- Pathfinding on a denser grid: `bool[512][512] free` at 5.0-unit spacing (`state.d:603-604`) — 2×2 nav cells per terrain cell.
- **Everything resident, always. All meshes drawn every frame; no culling, no streaming.**

**Comparison for the user:** the playfield is 255×255 cells — the 161×161 preview is **63% of Sacrifice's linear size and 40% of its area**, not "tiny". Zhaozhou's specified resident set (1,024 patches × 32×32 cells = **1,048,576 cells**) is **16× the cell count of a full Sacrifice map**. *Sacrifice's cells are coarse (10 m); its perceived hugeness comes from the skybox/void, not from cell count.*

## 2. Terrain representation

- **Per vertex:** height + void bool (`maps.d:8-11`). Normals derived **at load** by area-weighted face accumulation (`sacmap.d:386-410`) and **never touched again**.
- **Per cell:** one `ubyte` texture index 0–255 (TMAP). *That is the entire material model.*
- **Global:** LMAP — a 256×256 RGBA baked colour/light map draped over the island (one texel per vertex).
- **Files** (in a `.scp` WAD): `.LEVL` (44 B: souls/levels/god + 4-char tileset tag), `.ENVI` (sun/ambient/fog/specular/sky textures), `.HMAP`, `.TMAP`, `.LMAP`, `.SIDS`, `.NTTS`, `.TRIG`.
- **Mesh build** (`sacmap.d:438-536`): quads bucketed **by texture id** — up to 256 terrain meshes + bottom + edge. Each quad emits 4 unshared vertices; fixed diagonal 0-1-2/2-3-0, with alternate splits when exactly one corner is void (`getFaces`, `sacmap.d:375-382`). Per vertex: position, normal, `coord`=(i,j)/256 (global UV), `texcoord`=(0|1,0|1) (per-quad corner).

## 3. TEXTURING — the big one

**There is no inter-material blending. Zero. One material per cell, one primary texture sample per fragment.**

- Tileset = **256 textures of 64×64, CLUT8 with one shared palette** (`sacmap.d:354-371`: `LAND.PALT` + `0000.MAPT…0255.MAPT`). Six tilesets, selected by the LEVL tag.
- UVs: quad corners (0,0)…(1,1) with **GL_MIRRORED_REPEAT** (`dagonBackend.d:2247-2248`) — adjacent same-texture cells mirror into each other seamlessly, **no seam artefacts without any blending**.
- Transitions are **authored into the tileset**: the 256 ids include hand-drawn transition tiles organised into "map groups" (`MG%02d.MAPG`, 60-byte structs listing ≤14 member tile ids + a shared detail id, `maps.d:55-76`). *The 2000-era answer to splatting: paint the transition, don't compute it.*
- Modern shader per fragment (`terrain2.d:159-218`): `totalColor = 0.5*diffuse*(1.0+detail)*lmap` — **primary tile sample × global LMAP tint × optional detail bump**. Detail fades by squared eye distance (`detailFactor=1.5e-4`).
- LMAP sampled with the global `coord` UV — it carries baked AO/colour variation so the 64×64 tiles don't look repetitive.

**Portability: directly portable, and it VINDICATES Mosaic.** The load-bearing samples are exactly two: tile texture (CLUT8 64×64 — our native format) + LMAP tint. On our hardware: primary TMU = MAPT tile with mirrored repeat + mipmaps; restricted auxiliary sampler = the 256×256 LMAP (RGB565, bilinear) — **precisely the "one primary + restricted aux" budget already specified**. The detail/bump layer is the only shader-era addition; drop it or fold into mip dithering.

**Sacrifice's tilesets and our Mosaic are complementary, not rivals**: authored transition tiles give crisp painterly borders; Mosaic gives *procedural* borders without authoring 256 tiles. Support both — Mosaic picks the material id, and material ids may be transition tiles.

## 4. TERRAIN DEFORMATION — two-tier, cleaner than expected

**Tier 1 — transient analytic waves (their "field programs").** Erupt and Quake are pure functions f(x,y,frame) with hard finite support, no stored state beyond `{position, frame}`:
- `Erupt` (`state.d:3010-3069`): `range=50, height=15, growDur=4.2s, fallDur=0.15s, waveRange=90, waveDur=1.0s`. Growth = linear-in-time cone `0.6*(1-dist/range)` + raised-cosine cap `0.5*0.4*(1+cos(pi*dist/(0.8*range)))`; collapse launches an expanding annular wave `waveLoc=waveRange*progress; waveSize=(0.15+0.65*(1-progress)²)*range; if(wavePos<1) disp += 0.5*0.4*(1+cos(pi*wavePos))*height*(1-progress²)` plus a whole-area rebound dip.
- `Quake` (`state.d:4802-4838`): same wave grammar, `waveRange=50, waveDur=0.5, waveHeight=1.5, reboundHeight=2.0`.
- All raised-cosine + polynomial envelopes — **fixed-point friendly; one cos LUT covers everything.**

**Tier 2 — persistent delta layer.** `PermanentDisplacement` (`state.d:6784-6832`): `float[256][256]` **added on top of** the immutable base heightfield, with a crc32 hash recomputed on write for change detection. Stamps:
- Bombardment crater: ellipsoid cap, `dentRadius=15, dentHeight=1.2`, `disp += -sqrt(1-distSq/r²)*dentHeight`.
- **Volcano** (`state.d:4050-4113`) — the crown jewel. Cone shape is **data-driven**: a 33×33 ubyte stencil from the original game's `volc.DATA`, scaled ×0.2. `computeDisplacement` snaps to the cell grid, adds a plaza-flattening term `(target-cur)*(1-flatFalloff)` (`flatInner=80, flatOuter=164`, square falloff), then grows by **incremental scaling**: `applyDMapDelta(from,to)` adds `(to-from)*dmap[j][i]` per cell — so an **interrupted cast cleanly un-applies**, and after `spell.duration` it **decays to `residual=0.25`** of full height: a permanent scar that is a scaled-down volcano.

**Data path to screen:** each frame with deformation active, the engine composites a 256×256 R32F displacement texture on the GPU: permanent layer uploaded only when the hash changes, then erupt/quake waves **re-implemented in GLSL** as additive fullscreen passes with the same formulas. Terrain and shadow vertex shaders do `va_Vertex + vec3(0,0,texture(displacement, coord).r)` with NEAREST filtering.

**Consequences:**
- The mesh is **never rebuilt**. Deformation is a pure per-vertex Z offset at draw time.
- **Normals are NEVER updated** after deformation — craters keep pre-crater lighting, masked by the detail bump.
- Collision/gameplay: a CPU `Displacement` functor sums permanent + **every live effect** per height query — unbounded cost. Deterministic lockstep sim; GPU path is presentation-only.
- **Pathfinding ignores deformation entirely** — nav heights baked once at load (`state.d:665`).
- No dynamic retexturing: `// TODO: allow dynamic retexturing` (`sacmap.d:176`).
- **A GENUINE CPU/GPU DRIFT BUG FOUND**: Erupt `reboundHeight=2.0f` on CPU (`state.d:3020`) vs `3.0f` in the GLSL copy (`terrain2.d:382`) — **physics and visuals disagree**. Lesson: dual implementations of field programs rot. *Our single bounded-field-program evaluator feeding both sim and display is the correct design, and this is the proof.*

## 5. Rendering

**No LOD. No culling. No stitching problem — because nothing ever changes resolution.** `renderMap` binds the terrain material once and loops ≤258 texture-bucket meshes. Budget: ~130k triangles top surface + a full mirrored **bottom copy** at `mapDepth=50` + edge skirts, drawn again into shadow cascades — brute-forcing 300k+ triangles/frame on a desktop GPU. **Nothing portable here**; our per-patch LOD with crack-safe stitching is strictly better suited — sacengine never solved this because it didn't have to.

## 6. Floating islands / world edges

- Void cells = heightfield bit 15. Heights normalised so the lowest non-void vertex is 0.
- **Cliff skirts**: every ground/void boundary drops vertical quads `mapDepth=50.0f` straight down, textured by the per-map `envi.edge` TXTR with V∈[0,1] over the drop; diagonal-void cases get diagonal skirt quads (`makeEdge`, `sacmap.d:502-524`).
- **Underside**: a complete mirrored copy of the top surface at −50 with inverted normals and clamped texcoords.
- Below everything, an `undr` sky texture (part of the 5-texture sky set `sky_/skyt/skyb/sun_/undr` — **the same set our sky spec already implements**).
- **Directly portable and cheap**; the bottom copy can be a low-LOD single sheet rather than full resolution.

## 7. Where a purpose-built machine exceeds this ("more deformable than Sacrifice")

sacengine's ceilings, all structural:
1. **Z-only displacement** — no overhangs, no horizontal push. (Fundamental to both designs.)
2. **Deformation resolution = heightfield resolution** (10-unit cells). A crater of r=15 spans **3 cells**. Our 32×32-cell patches with finer cells already beat this.
3. **Per-query cost scales with live effect count**, unbounded. Our *bounded* field programs fix exactly this.
4. **Stale normals + stale pathfinding + no retexturing** after deformation. Re-deriving normals from displaced heights during patch mesh emit (finite differences, dirty-patch-only) and marking nav patches dirty would **visibly out-Sacrifice Sacrifice**.
5. Permanent layer has **no compression/sparsity** — a dense float plane with full-plane crc32 rescans per stamp. Our per-patch scar deltas with dirty flags are strictly better.

## What we should take — ranked

1. **Two-tier deformation: persistent additive delta + transient analytic wave programs (Erupt/Quake grammar).** *Directly portable.* The wave grammar is one cos LUT + polynomial envelopes in fixed point; implement `waveLoc/waveSize/wavePos` expanding-annulus + rebound-dip, with Erupt's constants as tuning starting points. **Improve on it: single evaluator for sim and display** (theirs already drifted, 2.0 vs 3.0).
2. **Incremental scaled stamping** (`applyDMapDelta(from,to)`): store the stamp once, apply `(to−from)×stamp` per tick; interruption = scale to 0; permanence = decay to a `residual` fraction. *Directly portable*, perfect for scar deltas — one stamp record replaces per-frame rewrites.
3. **Data-driven stamp stencils** (33×33 ubyte `volc.DATA`) + **plaza flattening with inner/outer falloff**. *Directly portable* — small integer asset tables; artists author craters/cones as tiny bitmaps.
4. **Tileset texturing: per-cell CLUT8 64×64 tiles with mirrored repeat + authored transition groups (MAPG).** *Directly portable* — our hardware's native diet, one primary sample, zero blending. Adopt MAPG grouping so Mosaic's per-pixel pick can select among a group's members for variation.
5. **Global low-res colour/light map (LMAP, 1 texel/vertex) as the aux sample.** *Directly portable* (RGB565, bilinear) — breaks tile repetition, bakes AO, cheapest possible second layer.
6. **Skirt + bottom-sheet + under-sky island edges** with void-bit and diagonal-void triangulation rules. *Directly portable*; simplify the bottom to low LOD.
7. **Displacement-as-texture composition.** *Portable if simplified*: no render-to-texture on fixed function — the equivalent is evaluating field programs per patch-vertex during the patch rebuild/LOD pass, **only for patches intersecting an effect's finite support** (all their effects have hard range cutoffs — exploit for dirty-patch culling).
8. **Do not copy:** monolithic no-LOD no-cull whole-map draw; per-height-query summation over all live effects; NEAREST displacement divorced from normal update; dense float delta plane with crc32 full rescans.
