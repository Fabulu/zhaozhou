# FINDINGS — R1: Sacrifice (2000) — Skies and God Beams (original game evidence)

**Run:** RUN-20260814-2015-sacrifice-sky-and-beams — **Date:** 2026-08-14 — **Agent:** RECON R1
*Persisted by orchestrator.*

## SUMMARY

1. **Team correction:** Lead programmer was **Martin Brownlow** (he originated the terrain engine idea), not Brian Polidori — no "Brian Polidori" connection to Sacrifice could be verified. Eric Flannum = lead architect/designer; Dan Liebgold = tools/AI. Only **two programmers** did the majority of coding. Sources: IGN interview (Aug 2000), RPS "The Making of: Sacrifice".
2. **Engine (verified, primary quotes):** custom Shiny engine; **real-time LOD** — polygon count continuously scaled per-frame against target framerate; texture size/mode auto-configured on first boot; min spec PII-300 + TNT1/Voodoo2 (fixed-function T&L era — this is why everything is fixed-function-reproducible); shipped map editor **Scapex**; maps 150–500 KB. Terrain = heightmap + tile map + **lightmap** (HMAP/TMAP/LMAP formats). Runtime tick 60 Hz.
3. **The sky is NOT a skybox.** Best evidence (tg-2's `sacengine` reimplementation, reverse-engineered from the game's data files): it is a **world-anchored 64-segment cylinder** (two horizon bands, `skyb` below horizon / `skyt` above, radius ~10,240 world units, centered on the map) **+ a flat "under" plane below the world** (what you see past the floating islands' underside) **+ a huge scrolling cloud quad above** (texture repeated 4×) **+ a small additive sun quad**.
4. **Sky animation constants (reimplementation):** cloud layer UV-scrolls one full tile per **64 s** in direction (1,−1); the whole cylinder+under-plane rotates about world Z at **2π/512 s (one revolution per 512 s ≈ 8.5 min)**; cloud layer brightness energy 1.7; sun energy 25 × 1.7 with luminance-keyed alpha; cloud alpha clamped by per-level `maxAlphaFloat`; clouds fade radially at the quad edge **and are punched out near the sun's projected position** (`sGap = min(1, 96·r²)`) so the sun "burns through" the clouds.
5. **Per-level sky data:** each level's `.ENVI` file names five 4-char sky texture tags (`sky_` (clouds, shared default), `skyt`, `skyb`, `sun_`, `undr`), plus fog (linear/exponential, nearZ/farZ/density), sun direction vector, sun color, sun fullbright color, ambient RGB, sky background RGB, shadow strength, landscape specularity/glossiness.
6. **Textures:** 8-bit **paletted** (`.TXTR` + `.PALT` external palette), optional explicit alpha channel or single alpha-color key; arbitrary w×h. Cylinder textures use **mirrored-repeat wrapping** (8 repeats around), cloud layer uses plain repeat.
7. **God beams ("sunbeams") are world-anchored additive mesh geometry, not billboards.** They are a material part (part 0 or 1) of ordinary level objects with tags `esb1`, `esb2`, `esb_` ("ethereal sunbeams" — the ethereal realm where the gods appear), `ea_b`/`ea_r`/`etfn` (ethereal altar), `pcsb`, `casb`, `st4a`. Rendered shadeless (unlit), **blending = Additive (SRC_ALPHA, ONE), depthWrite = false, energy = 4.0**. Depth test stays on → **terrain and units correctly occlude beams; beams never occlude each other or write depth**.
8. **Caveat:** all constants in points 3–7 are from a **GPL reimplementation** (`tg-2/sacengine` + `tg-2/dagon`) that reverse-engineered the original's data formats and observed behavior. **No Shiny primary source (postmortem, Gamasutra article, or leaked source) on sky implementation was found** — the claim is only that the *data formats* are original; the exact shader math is tg-2's faithful approximation.
9. **Era cross-reference:** Quake 3 = 6-face cubemap sky (view-locked); Unreal 1 = separate SkyZone room re-rendered through Fake Backdrop portals; Sacrifice = world-anchored cylinder + planes; era god beams = additive textured meshes/quads (Zelda OoT, DK2).
10. **240p fixed-function portability:** everything above is 100% fixed-function-reproducible — cylinder bands are gouraud/flat textured strips, clouds are one scrolling textured quad with per-vertex alpha for the radial fade, sun/beam additive blending is a fixed blend mode, and the sun-through-clouds "gap" can be baked into the cloud alpha or approximated with a second additive quad. No pixel shaders required anywhere.

## FULL FINDINGS

### 1. Rendering architecture (verified from period sources)

- **[IGN, "Sacrifice Interview", 2000-08-11](https://www.ign.com/articles/2000/08/11/sacrifice-interview)** — interview with Lead Programmer **Martin Brownlow**, Lead Architect **Eric Flannum**, Software Engineer **Dan Liebgold**. Direct technical quotes:
  - *"All scaling of polygon counts is done in real-time based on framerate. As your framerate drops from the one you requested in the options, so polygons are taken out of the scene until it rises to the correct level. As your framerate rises, polygons are added."* — continuous LOD.
  - First boot *"analyzes your machine … to set the default rendering options (texture sizes, resolution, which texture modes your card supports)"*.
  - Minimum spec: *"PII 300 with 64Mb of RAM and an 8Mb TNT-1 or Voodoo-2 equivalent accelerator card."* → fixed-function T&L-era hardware, no pixel shaders.
  - Map editor **Scapex**; finished maps *"150k to 500k"*.
- **[Rock Paper Shotgun, "The Making of: Sacrifice" (Kieron Gillen, 2007/2016)](https://www.rockpapershotgun.com/the-making-of-sacrifice)** — Eric Flannum: *"The inspiration was originally from our lead programmer, Martin Brownlow … He'd also had the idea for the Sacrifice terrain engine."* Team: *"We had two programmers who did the majority of the coding."*
- **[tg-2/sacengine](https://github.com/tg-2/sacengine/)** (D-language engine reimplementation, GPL-3) — reveals the original file formats: terrain `.HMAP` (heightmap), `.TMAP` (tile/texture map), `.LMAP` (**lightmap** — the landscape is lightmapped), per-level `.ENVI` (environment), `.TXTR`/`.PALT` (paletted textures), `.SAXS`/`.SXSK`/`.SKEL` (skeletal meshes/animations, 3D Studio Max pipeline), maps `.SAC`. Runtime simulation tick: `updateFPS = 60`.

**Could NOT verify:** "Brian Polidori" as programming lead — period sources consistently name Brownlow.
**Could NOT find:** any Game Developer/Gamasutra postmortem dedicated to Sacrifice's renderer — **no primary Shiny source on sky rendering exists online**; technical ground truth is the data-format-level evidence below.

### 2. The sky — reconstructed from `sacengine` (reverse-engineered, high confidence on structure)

**Per-level environment block** (`sacengine/source/envi.d`, parses original `.ENVI`, 264 bytes): `skyRed/Green/Blue` (background clear color), `sunDirection{X,Y,Z}`, `sunDirectStrength`, `sunAmbientStrength`, `ambient{R,G,B}`, `sunColor{R,G,B}`, `sunFullbright{R,G,B}`, `shadowStrength`, `landscapeSpecularity`, `landscapeGlossiness`, fog (`linear|exponential`, `fogNearZ`, `fogFarZ`, `fogDensity`, fog RGB), `minAlphaFloat/maxAlphaFloat` (alpha-blend clamps — maxAlpha applies to the cloud layer), and five 4-char texture tags: **`sky_` (cloud layer, shared default `SKY_.TXTR`), `skyt` (upper band), `skyb` (lower band), `sun_` (sun sprite), `undr` (under-world plane)**.

**Geometry** (`sacengine/source/sacobject.d`, class `SacSky`, constants `scaling = 4·10·256 = 10240`, `dZ = −0.05`, `undrZ = −0.25`, `skyZ = +0.25`, `relCloudLoc = 0.7`, `numSegs = 64`, `numTextureRepeats = 8`, `energy = 1.7`):

| Layer | Geometry | Size (×10240) | Texture |
|---|---|---|---|
| `skyb` lower band | 64-segment cylinder band, inner r=0.4 at z=−0.25 → outer r=0.5 at z=0 | 5120 outer radius | `skyb`, 8 repeats around, mirrored-repeat |
| `skyt` upper band | 64-segment cylinder band, r=0.5, z=0 → z=+0.25 | 5120 radius | `skyt`, 8 repeats, mirrored-repeat |
| `undr` under-plane | flat 1×1 quad at z=−0.25 | 10240×10240 | `undr` (per level) |
| `sky` cloud layer | flat quad at z=+0.175 (=skyZ·0.7), **UV 0..4** (4× repeat) | 10240×10240 | `SKY_.TXTR`, plain repeat (scrolling) |
| `sun` | 0.25×0.25 quad at z=+0.25 | 2560×2560 | `sun_` per level |

**Placement & animation** (`sacengine/source/renderer.d`, `renderSky`): the whole assembly sits at the **map center** — **world-anchored, never translated with the camera** (parallax is inherent: it's just huge relative to the islands). Cylinder + under-plane rotate about Z at `2π/512 rad/s`. Cloud quad doesn't rotate; its UVs scroll `cloudOffset = (t mod 64s)/64 · (1,−1)` — one texture tile per **64 seconds**. Rendered in the **transparent pass after opaque terrain**, depth-tested (islands correctly occlude sky).

**Cloud shader** (`dagon/src/dagon/graphics/materials/sacSky.d`): alpha-blended, depth-mask off; final alpha = `texture.a · alpha(clamped by envi.maxAlphaFloat) · (1−r²) · sGap` where `r` is normalized distance from cloud-quad center (radial edge fade) and `sGap = min(1, 96·|sunLoc−loc|²)` **punches clouds out around the sun's camera-projected position on the cloud plane**. RGB multiplied by energy 1.7.

**Sun** (`dagon/.../sacSun.d`): additive quad, energy 25·1.7, alpha = `min(3·luminance, 1)` — luminance-keyed so dark texels vanish: a lens-flare-style additive sprite, but **fixed to the sky, not screen space**.

### 3. God beams / light shafts

From `sacengine/source/sacobject.d` (comment: *"ethereal altar, ethereal sunbeams"*) and `dagonBackend.d`: specific level objects carry a **sunbeam material part**: tags `esb1`, `esb2`, `esb_` (ethereal sunbeams), `ea_b`, `ea_r`, `etfn` (ethereal altar set), `pcsb`, `casb`, `st4a` (part 1). Rendering: **shadeless (unlit) mesh geometry, blending = Additive (SRC_ALPHA, ONE), depthWrite = false, energy = 4.0**.

They are ordinary **world-anchored meshes** — no billboarding, no screen-space lens-flare behavior; the additive falloff is painted into the mesh texture. **Occlusion is correct by construction**: depth test on means terrain, units, and islands passing in front clip the beam; `depthWrite=off` means beams never occlude anything (including each other). In the original game they're most prominent in the **ethereal realm** (the between-mission god area) and as set-dressing on altars/pyres.

### 4. Era cross-reference

| Technique | Example (era) | Visual signature | 240p fixed-function portability |
|---|---|---|---|
| 6-face cubemap, view-locked | Quake 3 `skyParms` farbox — [Q3 shader manual](https://icculus.org/gtkradiant/documentation/Q3AShader_Manual/ch03/pg3_1.htm) | Perfect parallax at infinity; seams if mipped wrong | Good, but 6 textures + cube UV mapping costs tile budget |
| SkyZone room + Fake Backdrop | Unreal 1 / UT ([BeyondUnreal wiki](https://beyondunrealwiki.github.io/pages/skybox.html)) | Sky can contain animated movers; rendered "before everything" via portal | Poor fit (needs second scene render) |
| World-anchored cylinder + planes | **Sacrifice** | Distinct horizon band, parallax only from real camera travel, clouds scroll overhead | **Excellent** — textured strips + quads, no per-pixel work |
| Sky dome with vertex-color gradient | Myth-style / flight sims | Smooth zenith→horizon gradient | Excellent |
| Additive mesh "god beam" | Sacrifice sunbeams; Zelda OoT temple shafts; DK2 rooms | Soft shafts, brighten what's behind, occluded by world, never occlude | **Excellent** — exactly one fixed blend mode |
| Lens-flare sprite (screen-space, occlusion-tested) | Common 1998–2000 FPS | Flare tracks light's screen position, pops off when occluded | Needs occlusion query — avoid at 240p |

### 5. Concrete parameters worth reproducing at 240p

- **Sky structure:** 64-segment cylinder in two bands (below/above horizon) with 8 mirrored texture repeats around; one flat under-plane (critical for floating-island levels — looking off the island edge shows the under-world texture, not void); one overhead cloud quad with 4× texture repeat; one small sun quad.
- **Scale:** sky radius ≈ 4× the map half-width; cloud plane at ≈ 0.7 of upper-band height; sky center anchored to map center at fixed world Z.
- **Motion:** cylinder rotation 1 rev / 512 s (imperceptibly slow — could be dropped at 240p); cloud scroll 1 UV tile / 64 s diagonal. Deterministic from frame count (`frame % (64·60)`), no floating drift — perfect for the console's determinism requirement.
- **Cloud layer:** alpha-blended, alpha clamped by per-level maxAlpha, radial edge fade `α·(1−r²)`, sun punch-out radius `r² < ~1/96` around sun's projected position; brightness ×1.7.
- **Sun/beams:** additive (SRC_ALPHA, ONE); sun alpha = min(3·luminance, 1), energy ~42; beams energy 4.0, unlit, depth-write off.
- **Per-level palette of knobs (ENVI-equivalent):** sky background RGB, fog type/near/far/density + RGB, sun direction + color + fullbright color, ambient RGB, shadow strength. These five texture tags + colors are what made each god's realm feel distinct.
- **Occlusion contract:** sky drawn after opaque world, depth-tested, depth-write on for bands, off for sun/clouds/beams. Beams never fade when geometry passes in front — they are *clipped*, which reads as correct occlusion.

### 6. Flags — what we do NOT actually know

1. **No primary Shiny source on sky rendering exists.** Everything structural comes from `tg-2/sacengine`/`dagon` reverse-engineering. Structure (layers, formats, ENVI fields, object tags) is almost certainly authentic — it parses real game files. Exact shader math (energy values, 96·r² sun gap, gamma) is tg-2's approximation. **Design from the structure; treat the constants as art-direction starting points, not gospel.**
2. **"Brian Polidori" unverifiable** — lead programmer was Martin Brownlow.
3. **Original TXTR resolutions unverified** (format allows arbitrary w×h).
4. Whether the original hardware path used the exact same two-pass cylinder (vs. some single-mesh dome) is unknowable; visually equivalent either way.
5. Beam behavior follows from object/material data, not first-hand video analysis.

**Sources:** [IGN Sacrifice Interview (2000)](https://www.ign.com/articles/2000/08/11/sacrifice-interview) · [RPS The Making of: Sacrifice](https://www.rockpapershotgun.com/the-making-of-sacrifice) · [tg-2/sacengine](https://github.com/tg-2/sacengine/) · [tg-2/dagon](https://github.com/tg-2/dagon) · [Q3 Shader Manual](https://icculus.org/gtkradiant/documentation/Q3AShader_Manual/ch03/pg3_1.htm) · [Quake3World skybox thread](https://quake3world.com/forum/viewtopic.php?t=19009) · [BeyondUnreal Wiki: SkyBox](https://beyondunrealwiki.github.io/pages/skybox.html) · [MobyGames: Sacrifice](https://www.mobygames.com/game/2861/sacrifice/) · [Steam: Sacrifice editor thread](https://steamcommunity.com/app/38440/discussions/0/613937943036889690/)
