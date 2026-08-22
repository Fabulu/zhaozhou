# RECON N3 — Noctis IV: Rendering Architecture & the "From Space" Look

*Fable recon agent, 2026-08-15. Sources: `C:\programmieren\noctis\niv-lr` (modern port, faithful line-for-line translation of the DOS asm), `niv-plus` (original Borland C++/inline-asm DOS source), `harness`. Persisted by orchestrator.*

**What `harness` is:** `C:\programmieren\noctis\harness\` is a **prior verification rig from an earlier wave of this project**: `oracle.c` (galaxy-hash ground truth lifted from `niv-lr`), `python.bin` (independent arbitrary-precision implementation), `lino.bin` (L.in.oleum implementation with an inline IMUL fragment), compared bit-exactly three ways by `compare3.py`. Per `oracle.c:1-31`: *"There is no star table anywhere — the galaxy is this function."* **The starfield hash is already validated for reimplementation on our stack.**

## 1. Renderer architecture

Pure software renderer, one 320×200×8bpp hidden page, VGA mode 13h, 6-bit DAC. `noctis-d.h:90-91`; 3D viewport 306×180 (`noctis-d.h:104-107`). Whole-game RAM budget **550 KB**, itemized at noctis-d.h:23-67 (`st_bytes 64800` = 360×180 sky texture; `pl_bytes 65552` = planetary map).

**Polygon engine** ("The Third Flare", `tdpolygs.h:1-33`): flat-filled and textured **convex quads** (`VERTEXES_PER_POLYGON 4`). Self-reported *"an average of 12,000 polygons per second … 75MHz Pentium"*. Per polygon (`poly3d`, tdpolygs.h:344-541): rotate/translate with **precomputed sin/cos products** (`opt_pcosbeta = cos(beta)*dpp`, recomputed only when the camera turns, 303-314) → 6 multiplies/vertex, no trig; near-plane clip at `uneg = 100` with a fast-load path skipping all clipping when every vertex is in front (384-388, *"that is almost always"*); project `x = (1/z)*X + x_center` (483-501); `dpp = 210` (NOCTIS.CPP:2214) → horizontal FOV ≈ 72°.

**Fill (original asm):** trace polygon borders in sentinel colour **255**, then per scanline `repne scasb` to find borders and `rep stosd` to fill between (`TDPOLYGS.H:1469-1577`). No edge tables, no sorting — **the framebuffer itself is the edge list**.

**Texture mapper** (`polymap`, tdpolygs.h:612-1351): scanline quad mapper over **256×256, 64-intensity-level, headerless textures**. Perspective correction **once per 16-pixel span**; inside a span u/v step linearly in **8.8 fixed point**: `fakesi = (v - tempv) >> 4`, texel address = two high bytes packed `tbx = (tbh<<8)+tbl` (996-1023). LOD switches: `culling_needed` (2 px/texel), `halfscan_needed` (duplicate every other scanline).

**Compositing modes** per polygon: 0 opaque; 1 **additive translucency** (add to dest's low 6 bits, clamp 62 — `TDPOLYGS.H:1601-1611`); 2 bright/light-shaft; 4 per-scanline fading gradient. `polymap` adds transp/bright/merger/bumper; HUD text uses `dest&0x3F + texel`, clamp 0x3E, **keeping dest's ramp bits 0xC0** (1050-1077).

**Frame budget:** `sync_stop()` busy-waits on `clock()` (noctis-0.cpp:5643-5648, *"up to 18 frames per second on any PC"*); `FRAME_TIME_MILLIS 55`. **The whole aesthetic was built for 18.2 Hz on a 386/486.**

**Space-frame pipeline order** (noctis.cpp:3128-3759): clear or `pfade` decay → white_globe suns + lens flares → `psmooth_grays_ex` glow blur → `mask_pixels_ex` shifts into sky ramp (64..127) → `globe()` near star → `draw_planets()` → cockpit polygons → **`psmooth_64_ex` twice** → `sky(0x405C)` starfield **last** → HUD (unsmoothed).

**How it stayed fast:** byte-index arithmetic on one 64,000-byte buffer; trig hoisted out of loops; per-16px perspective; prebaked projection maps instead of 3D spheres; painter layering instead of any depth buffer; **the palette does all the colour work**.

## 2. Starfield

`sky()` at `noctis-0.cpp:2311-2490`. *"2,744 stars. Star magnitudes go from 0 to +13."*

- **Procedural, infinite, table-free.** 100,000-unit cubic sectors; each holds ≤1 star whose position is a **pure integer hash** of sector coords: `temp_x = ((sect_x+sect_z) & 0x0001FFFF) + sect_x - 50000`, then two folded 32×32→64 signed multiplies (`edx += eax` after `imul`) feed y and z (2376-2419). Hashing to exactly 50000 = "no star". **This exact arithmetic is bit-exactly verified in `harness`.**
- **Counts/culling:** visible cube 9³=729 sectors, 14³=2744 amplified. Rarity rises away from the galactic core: `rarity_factor = (1 << (int)(distance*0.25e-8)) - 1;` skip if `((x+y+z) & rarity_factor)` (2349-2357). Y-distance counts 30× (crushed-disc galaxy).
- **One pixel per star, additive magnitude:** `mask = 63 - (rz >> (13 + field_amplificator))`; add to dest's low 6 bits, clamp 63, keep ramp bits (2459-2470). Brightness purely distance-based.
- **Occlusion for free:** stars plot only if the destination index is in range — `sky(0x405C)` in space (only over dim sky-ramp indices), `sky(0x003E)` for night skies. Anything already drawn masks the stars. **Zero depth testing.**
- **No twinkle. Motion streaks instead:** during fast flight the frame isn't cleared; `pfade(adapted, 180, 8)` subtracts 8 from every pixel per frame (noctis-0.cpp:353-371) → decaying warp trails.

**Verdict: directly portable, highest priority.** Pure int32, harness-validated, 2744 ops is nothing at 60 Hz.

## 3. Planets and atmospheres from space

**LOD ladder** (`draw_planets`, 4830-5245, thresholds 4917-4928): d < 250·r → rings; < 100·r → disc; < 25·r → full procedural surface map. Beyond → a point.

- **Distant dot with glow** (`far_pixel_at`, 3135-3225): brightness `64 - dist*0.384 + radius*1228.8`; fixed glow kernel — 4-neighbours `>>1`, diagonals `>>2`, distance-2 `>>3`, outer ring `>>4` (3173-3213), additive-saturating in-ramp. Three modes: LIGHT_EMITTING, **LIGHT_ABSORBING (halve dest — dark dust!)**, MULTICOLOUR. *Directly portable — a 13-tap sprite.*
- **Sphere rendering — the crown jewel. There is no 3D sphere geometry anywhere.** `globes.map` (22,586 B, "convex sphere in QT-VR") is a prebaked list of signed byte pairs (dy,dx): for each texel of the 360×180 surface texture, its screen offset on a 100-px-radius sphere; `dy == 100` escapes = "skip dx texels" (far hemisphere). `globe()` (2511-2642) walks the texture linearly, plots each texel at `center + (dy,dx)*mag_factor`, point size 1×1…4×4 by mag thresholds 0.33/0.66/0.99, max mag **1.32**. **Planet rotation = the start offset into the texture** — spinning a planet is free.
- **Terminator baked into the texture**: a **130° band** (not 180 — *"due to the diffused light and the reduced field at the edges of the globes"*, 4171-4176) starting at `plwp + 35`, darkened by `p_background[j] >>= 2` across all 180 rows (4650-4668).
- **Crescent at medium range** (`glowing_globe`, 2669-2754): same map, every 4th texel column, two flat colours — lit 127, dark `((color&0x3F)>>2)|(color&0xC0)`.
- **Atmosphere rim — the "glass bubble"** (globe() epilogue, 2644-2662): after the disc, a ring of circular **local blur spots** around the limb: spot radius `mag*7.25` px, ellipse `rx = mag*110` (**10% outside the 100-px disc**), vertical semi-axis `0.833*rx`, angular step `(1.2°)/mag`. Each spot is `smootharound_64`, a 2×2 in-ramp box blur inside a circle. **The limb glow is not additive light; it is diffusion** — the bright disc edge smeared outward into dark sky. Enabled only for thick-atmosphere planet types 2,3,6,8,9.
- **Clouds:** half-resolution overlay (`objectschart`, values 0x00–0x1F) from `cirrus` (add spot, clamp 0x1F), `atm_cyclon` (spiral: angle += 6°, radius decays), `storm` (concentric circles), added onto the surface texture before drawing. Cloud positions incorporate `secs / (random period)` so **weather drifts in real time**.
- **Surface textures** (`surface()`, 4178-4694): 360×180×6-bit, seeded from orbital elements, base noise from a 16-bit **middle-square hash** — `seed += i; result = seed*seed; seed = hi16 + lo16; texel = seed & 0x3E` — then per-type stacks of box smooths, volcano/crater/fracture/band/wave/contrast/negate. 10 planet types.
- **Suns:** close up, `globe()` with a 360×180 star texture (`load_starface`, middle-square + class-dependent smoothing, 5656-5682), and **animated convection by incrementing every texel `(v+1) % 64` each frame** while preserving ramp bits (`NOCTIS.CPP:3779-3784`). At range, `white_globe`/`white_sun` (2761-2967): analytic disc + radial falloff, `pix = 0x3F - (sqrt(x²+y²) - fgm) * (0x3F/(mag-fgm))`, full 0x3F inside `fgm = fgm_factor*mag`, additive clamp 0x3F; `white_globe` plots 2×2 blocks, `white_sun` 1×1. `fgm_factor` 0.3 for star corona, 0/0.5 for surface suns.
- **Rings are particles:** concentric jittered circles of `far_pixel_at` dots using a `lft_sin/lft_cos[361]` 1° LUT, `interval = 0.0075*ringray`, random gaps, randomly light-absorbing (dark dust lanes) vs emitting. `ringlayers = 0.05*(d3/ray)`.
- **Lens flares** (`lens_flares_for`, 2990-3071): 180 additive lines through the light's screen position, lengths oscillating via `l *= u; if (l>3||l<1) u = 1/u`, plus mirrored mini-flares stepping toward screen centre (`xr = xs*-0.1`, scale chain ×4/×3). Occlusion by probing the framebuffer index at the light position.

## 4. Nebulae / galactic background

Deep space is **black + procedural stars only** — no galaxy skybox; the "milky way" impression comes entirely from the sector-hash density gradient. Planet-surface skies use the concave QT-VR map `offsets.map` (7,340 B; each entry paints a 5×5 block) over `s_background`, generated per-planet by `nebular_sky()` (middle-square noise + smooth passes → alien striated nebulae) or `cloudy_sky(density, smooths)` (random ellipses, `b = (1.4142/sqrt((x+r)²+(y+r)²))*64` additive falloff). *Directly portable — offline-generatable 360×180 CLUT textures.*

## 5. Colour & palette discipline — THE CORE INSIGHT

Palette map (original comment, `NOCTIS.CPP:2217-2221`): 0–64 vehicle/HUD; 64–128 cosmos/sky; 128–192 stars; 192–256 planets.

**Every pixel index = 2-bit ramp selector (bits 7:6) + 6-bit intensity (bits 5:0).** Every blend, glow, smooth, fade and star is **saturating arithmetic on the 6-bit intensity, masking 0xC0 to preserve the ramp** (`&0x3F … &0xC0` appears in every effect). **Colour never exists at draw time; the DAC colourises at scanout.** That is why a 256-colour game can afford additive halos everywhere.

- Ramps built by `shade()` linear gradients. Space-sky ramp 64–127: black → tinted mid → slightly blue white, star tint from `class_rgb[12]` (class 0 = `63,58,40`; pulsar = `0,63,63`), desaturating with distance (`satur = 6.4*dsd/ray` — *"stars appear generally all white unless approached really closely"*). Planet ramps: 4×16-step gradient black → 0.25c → 0.75c → 1.25c → white.
- **Whole-scene grading is free**: `tavola_colori` applies multiplicative R/G/B filters (0–63) at DAC upload; used for fade-ins, lightning flashes, sky tint by star class, smooth per-frame palette interpolation.
- **"Dithering"**: not ordered — the full screen gets `psmooth_64_ex` **twice per frame** (comment: *"Anti-aliasing e dithering (error-diffusion) … extraordinarily beautiful effects on a screen that is poorly resolving both physically and chromatically"*): each pixel becomes the average of a 2×2 neighbourhood's intensities, keeping its ramp. **This soft in-ramp diffusion is a large part of the hazy, organic Noctis look.**
- The DAC is 6 bits/channel = 64 levels — the intensity math and colour depth line up exactly, and map cleanly onto RGB565.

**Verdict: directly portable, and it should be our foundation.**

## 6. Integer / fixed-point machinery (mirrors of our own)

| Mechanism | Where | Detail |
|---|---|---|
| 1° sin/cos LUT | noctis-0.cpp:2974-2988 | `lft_sin/lft_cos[361]` |
| Row-offset LUT | TDPOLYGS.H `riga[]` | y×320 per scanline |
| 8.8 fixed-point UV | tdpolygs.h:996-1023 | per-16px span; texel = `(v_hi<<8)|u_hi` |
| Galaxy hash | noctis-0.cpp:2376-2419; harness/oracle.c | signed 32×32→64 imul, hi+lo fold, `&0x1FFFF`, cutoff 50000 — **bit-exact spec already exists** |
| Middle-square noise | 5656-5668, 4232-4241 | `seed += i; seed = hi16(seed²)+lo16(seed²); out = seed & 0x3E` — generates every texture in the game |
| Cheap PRNG | `fast_srand/fast_random(mask)` | mask-ranged |
| Precomputed rotation products | tdpolygs.h:291-314 | `cos*dpp` folded so projection needs no divide |

The 3D transform is float (387 FPU required), but **everything hot beneath it is integer/LUT**.

## Ranked "what we should steal"

1. **Ramp×intensity CLUT discipline** (2-bit tint + 6-bit intensity; all effects = saturating 6-bit adds). *Directly portable.* The single enabler for glows, additive stars, flares, terminators, fades in fixed-function hardware.
2. **Sector-hash procedural starfield** with distance-magnitude and rarity gating. *Directly portable* — pure int32, already three-way verified in `harness`.
3. **QT-VR sphere offset maps** for planets/suns (offset-pair ROM + linear texel stream; rotation = start index; scale = one multiply; 1–4px point sizes). *Directly portable*, replaces all sphere rasterisation.
4. **Terminator baked in texture space** (`>>2` over a 130° band) + two-tone crescent at mid range. *Directly portable.*
5. **white_sun analytic disc + radial halo** and the **far_pixel 13-tap glow kernel**. *Directly portable* (sqrt → r² LUT).
6. **pfade motion streaks** — decay the framebuffer by 8/frame instead of clearing. *Directly portable*; spectacular for its cost.
7. **Sun-surface convection** — `texel = (texel+1) & 63` per frame. *Directly portable.*
8. **Middle-square noise + box-smooth texture synthesis.** *Directly portable*, ideal for load-time generation.
9. **Rings as jittered particle circles** with emitting/absorbing modes. *Directly portable.*
10. **Glass-bubble atmosphere rim** — blur ring at 110% disc radius. *Portable if simplified*: additive precomputed radial-falloff annulus instead of read-modify-blur.
11. **Index-range star occlusion.** *Portable simplified* — compositor ordering or readback.
12. **DAC filter grading** (per-frame multiplicative RGB on the CLUT). *Directly portable.*
13. **In-ramp 2×2 diffusion post-pass** (the "Noctis haze"). *Portable if desired* as a line-buffer op; our ordered dither partially replaces its function.
14. **LOD ladder policy** (dot → glow → crescent → disc → mapped disc at 250r/100r/25r). *Directly portable.*
15. **The polygon engine itself**: *not portable as-is* (float, framebuffer-scan fill); steal only its principles — hoisted rotation products, span-linear fixed-point UV — which our machinery already embodies.

**The big lesson:** Noctis achieves its entire "from space" look with **zero 3D geometry for celestial bodies** — prebaked projection maps, one-pixel additive stars, analytic radial discs, and palette arithmetic on a single byte buffer at 18 Hz. At 240p/60 Hz with hard integer math, we are strictly richer than its native habitat in every dimension except CPU generality — and it barely uses any.
