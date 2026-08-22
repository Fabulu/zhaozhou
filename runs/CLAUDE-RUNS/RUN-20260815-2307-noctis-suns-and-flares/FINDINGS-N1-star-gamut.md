# RECON N1 — Noctis IV Star/Sun Rendering (the gamut)

*Fable recon agent, 2026-08-15. Sources: `niv-plus/source/` (Borland C++ DOS, closest to original), `niv-lr/src/` (modern C++ port — same algorithms, asm rewritten readable). **The two flavors agree on every constant below.** Persisted by orchestrator.*

**Architectural fact, stated up front:** Noctis never computes a star's colour per pixel. Everything is drawn as 6-bit **intensity** (0..0x3F) into a 256-colour framebuffer, and the VGA palette — rebuilt per frame from tiny formulas — turns intensity into colour. Disc, corona, sky, flare and cockpit lighting are all "coloured" by CLUT ramps. **This is exactly what a palette/CLUT FPGA console wants.**

## 1. Star classification — the gamut

12 classes (`star_classes 12`, noctis-d.h:123). Tables at `NOCTIS-0.CPP:893-930` (= niv-lr noctis-0.cpp:688-709); undertones at `NOCTIS.CPP:3679-3740`; surface-light factor `dfs` at `NOCTIS-1.CPP:2774-2787`. RGB are VGA 6-bit (0–63).

| # | Name | `class_rgb` R,G,B | undertone ir2,ig2,ib2 | `class_ray` | `class_rayvar` | max planets | spin |
|---|---|---|---|---|---|---|---|
| S00 | Yellow star | 63,58,40 | 64,54,28 | 5000 | 2000 | 12 | 0 |
| S01 | Blue giant | 30,50,63 | 36,50,64 | 15000 | 10000 | 18 | 0 |
| S02 | White dwarf | 63,63,63 | 24,32,48 | 300 | 200 | 8 | rnd(4)+1 |
| S03 | Red giant | 63,30,20 | 64,24,12 | 20000 | 15000 | 15 | 0 |
| S04 | Orange giant | 63,55,32 | 64,40,32 | 15000 | 5000 | 20 | 0 |
| S05 | Brown dwarf | 32,16,10 | 28,20,12 | 1000 | 1000 | 3 | 0 |
| S06 | Grey giant (dead) | 32,28,24 | 32,32,32 | 3000 | 3000 | 0 | 0 |
| S07 | Blue dwarf | 10,20,63 | 32,44,64 | 2000 | 500 | 1 | rnd(12)+1 |
| S08 | Multiple system | 63,32,16 | 64,60,32 | 4000 | 5000 | 7 | 0 |
| S09 | Infant star (nebula) | 48,32,63 | randomized per identity | 1500 | 10000 | 20 | 0 |
| S10 | Runaway star | 40,10,10 | 32,26,22 | 30000 | 1000 | 2 | 0 |
| S11 | Pulsar | 0,63,63 | 36,48,64 | 250 | 10 | 5 | rnd(30)+1 |

`ray = (class_ray + random(class_rayvar)) * 0.001` (NOCTIS-0.CPP:3973) — pulsar 0.25, runaway giant 30 → **the gamut spans ~120×**.

Class → look beyond colour:
- **Spin**: classes 11/7/2 get a rotation rate; others rotate by wall-clock `(clock()/360)%360`.
- **Texture smoothing**: compact stars (11/7/2) get 2–4 extra blur passes → smoother, less granulated discs.
- **Flare suppression**: dark stars (5 brown dwarf, 6 grey giant, 10 runaway) **never** get lens flares.
- **Pulsar duty cycle**: class 11 flares/illuminates only while `gl_start < 90` — **one quarter of each rotation**; spin 1–30°/frame sets blink rate; cockpit light ×5 during the flash.
- **Surface daylight factor `dfs`** per class: 1.0, 1.5, 0.5, 0.8, 1.2, **0.1, 0.1**, 0.4, 0.9, 1.3, 0.5, **0.2** — brown-dwarf/dead-star worlds are dim, blue-giant worlds glaring.

## 2. The star disc

**A point-splat textured sphere, not a polygon mesh.** `globe()` (NOCTIS-0.CPP:3043):
- `GLOBES.MAP` = precomputed `(dy,dx)` signed byte pair per texel of the visible hemisphere (orthographic sphere within ±100 px); sentinel `dy==100` = "skip N texels". Render = `screen = center + offset*mag_factor`, plot 1×1…4×4 blocks by magnification tiers (>0.33/0.66/0.99, clamp 1.32 → **max disc radius 132 px**). No per-pixel trig, no perspective inside the disc.
- **Texture = seeded noise** (`load_starface`): `seed = identity*12345`, per texel `ax += counter; ax = hi16(ax²)+lo16(ax²); texel = ax & 0x3E`, then 0–2 blur passes (+2–4 for classes 11/7/2). **That's the granulation.**
- **Rotation**: `start` = byte offset into the texture ring; `gl_start += nearstar_spin; gl_start %= 360`.
- **Granulation animation**: every frame each texel steps `(v+1) % 64` keeping top 2 bits — **palette-index cycling makes the surface boil at zero drawing cost**.
- **Distance washout**: `satur = (12*dsd)/ray` floored per-star, capped 63; each texel `max(texel, satur)`. Far → saturates to solid white; close → full granulation contrast. (Deliberate: stars are near-white until very close, "beyond the eye's saturation level".)
- **Colour** = texel `| colormask(64)` → indices 64–127, a 64-entry ramp rebuilt each frame:
  ```
  shade(64+ 0, 24, 0,0,0,      ir2,ig2,ib2);  // black → convection undertone
  shade(64+24, 16, ir2,ig2,ib2, ir,ig,ib );   // undertone → class colour
  shade(64+40, 24, ir,ig,ib,   64,70,76 );    // class colour → over-white
  ```
  floored by `satur = 6.4*dsd/ray` cap 44, endpoints **slewed 1 unit/frame** so palette changes never pop. **This 3-piece ramp is the entire limb-darkening/colour-gradient system.**
- **No geometric edge falloff on the disc** — hard edge, softened only by the corona underneath and the background blur.

## 3. Corona / glow / halo

Draw order in space: fade/clear → **corona** → companion glows → **lens flare** → blur (`psmooth_grays`) → `mask_pixels(+64)` → **disc on top** → far-point fallback.

**Corona** = `whiteglobe` (NOCTIS-0.CPP:3298): additive radial gradient at **3× star radius** with **30% flat-bright core**:
```c
mag = (mag_factor/rz)*100 + 1.5;    // apparent radius px
fgm = fgm_factor * mag;             // flat core radius (0.3 for stars)
ise = 0x3F / (mag - fgm);
zz = xa*xa + ya*ya;                 // ya steps 1.2/row → aspect-corrected
if (zz < mag²) pix = (zz > fgm²) ? 0x3F - (sqrt(zz)-fgm)*ise : 0x3F;
target[p] = min(target[p] + pix, 0x3F);   // ADDITIVE, clamped
```
Drawn in 2×2 blocks (half res). Because the background is then shifted into the 64–127 star ramp, **corona intensity is colourised by the same CLUT as the disc** — the glow is automatically the star's colour fading to black. Companions use `(3*ray, 0.15 - rand*0.3)`.

**Nebulous smear**: while moving, background is faded not cleared (`pfade(180,8)`) and box-blurred (`psmooth_grays`, 4×4 mean) — free motion-glow.

## 4. Distance LOD ladder (in units of the star's radius `r`)

| Distance | Drawn |
|---|---|
| `< 0.44r` | forbidden — autopilot pushes ship back |
| `< 8r` | textured `globe()` disc (max 132 px), granulation animated, full contrast |
| `< 100r` | + `whiteglobe` corona at 3r, 30% core |
| `5r … 1000r` | + lens-flare ray fan (`k = d/r` px spikes, sparser with distance) |
| `100r … 1550r` | far-point fallback: `far_pixel_at` ×3, colour `0x30 + (1600r-d)/(100r)` → **never dimmer than 75%** (minimum-visibility clamp) |
| interstellar | only in the procedural starfield: additive point `63 - (rz >> 13)` |

## 5. From space vs from a planet surface

On foot the sun is **not** the textured globe — it's `whitesun` (same math, full-res 1×1 px):
- **With atmosphere**: `whitesun(sun, 4*nray1, 0.0)` — `fgm=0` → **no flat core, pure radial glow ball** 4× radius. *The atmosphere "is" one parameter.*
- **Airless**: `whitesun(sun, 3*nray1, 0.5)` — hard 50% core + halo: crisp vacuum sun.
- Sun drawn as intensity into the sky layer, then shifted into **sky ramp 64–127**: airless → ramp to near-white; day sky → black→sky colour × class factor `dfs` × brightness. **The sun's tint on a surface is literally the top of the sky gradient** — a venusian sky tints the sun for free.
- Hidden when night or `rainy >= 2.5`. Surface flare band `10r ≤ d < 1000r` with **condition 2**: flare only if the pixel at the sun's screen position is a sky pixel (64..127) — free occlusion by terrain/clouds.
- **Everything the star shines on is palette-tinted by it**: cockpit ramp from star RGB/distance/eclipse; planet palettes blend 50/50 with star colour `(planet*2 + star)>>1`; bodies beyond the 4th orbit dimmed `brt = 64 - 4*(n-4)`.
- **Sun glints on water/ice** keyed to `xsun_onscreen`.

## 6. Determinism

- **Star existence & position**: 100,000-unit sectors, ≤1 star each, integer multiply-fold hash of sector coords; coordinate==0 → no star.
- **Identity seed**: `srand(x/100000 * y/100000 * z/100000)`, then class/radius/spin drawn in fixed order. Surface texture seed = `identity * 12345`.
- **Fast PRNG**: seed `|= 3`; `eax = seed²(64-bit); al += dl; seed += eax; return eax & mask` — all integer.
- **Every visual property of a star is reproducible from its integer sector coordinates alone.**

## What we should steal — ranked, with portability verdicts

1. **Intensity-plus-CLUT star colouring** (6-bit intensity; 64-entry ramp black→undertone→class-colour→over-white does disc, corona, glow and nebula in one stroke; slew endpoints 1/frame). — **Directly portable**; native CLUT territory. Two colours per class = 12×6 bytes of ROM.
2. **Additive radial corona with flat core** (`pix = 63 - (dist-core)*k`, saturating add). — **Portable if simplified**: replace per-pixel sqrt with an r²→intensity LUT; one parameter (`fgm` 0/0.3/0.5) switches atmosphere-glow / space-corona / vacuum-sun. Enormous payoff per gate.
3. **Palette-cycling granulation** (seeded noise; animate by incrementing texel index mod 64 — or on FPGA **rotate the CLUT** and touch zero pixels). — **Directly portable**, cheaper than the original.
4. **`satur` distance washout** — distant suns go white-hot, close suns reveal boiling detail. — **Directly portable**: one saturating max.
5. **Offsets-map sphere** (precomputed (dy,dx) splat list, 1×1..4×4 blocks). — **Portable if simplified**; integer-only, no per-pixel divider, ROM-friendly sentinel-skip encoding.
6. **LOD ladder with minimum-brightness clamp** (far point never dimmer than 0x30). — **Directly portable**; shifts/compares against `ray`.
7. **Pulsar duty-cycle strobe** (`gl_start < 90` gates flare *and* multiplies scene light ×5) + per-class spin. — **Directly portable**; a comparator. Cheapest exotic object ever shipped.
8. **Sun-as-top-of-sky-ramp on surfaces** — atmosphere tint, night dimming, lightning inversion all fall out of palette writes. — **Directly portable**.
9. **Sector-hash universe + fixed-order seeded draws**. — **Directly portable** (one DSP for 32×32 multiply); replace Borland `rand()` with our own documented LCG everywhere.
10. **Lens-flare line fan with ×1.5 length oscillation** + screen-pixel occlusion test. — **Portable if simplified**: bake 8–16 spikes into sprites; keep the 1-pixel occlusion read and the "spikes grow with distance, rays thin out" parameterisation.
11. **Star-tints-the-world palette blending**. — **Directly portable**, free mood lighting.
12. **Motion glow via fade-not-clear + 4×4 blur**. — **Portable if simplified**: bounded low-res blur on the background layer only.

Not portable as-is: the double-precision camera transform (use wide fixed-point; the visual math is already integer), and Borland `rand()` (replace, don't emulate — unless NIV universe compatibility is a goal, in which case emulate exactly: it's a 32-bit LCG).
