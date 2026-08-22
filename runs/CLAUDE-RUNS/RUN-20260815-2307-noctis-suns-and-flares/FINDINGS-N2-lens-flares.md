# RECON N2 — Noctis IV lens flares, glare & bloom

*Fable recon agent, 2026-08-15. Persisted by orchestrator. Bonus source: `C:\programmieren\Linoctissite\public\linojava\intrinsics\noctis.js:6631-6690` is a bit-exact JS port of the flare fan — confirms every constant (e.g. `scale = float32FromBits(1069547520)` = **1.5**).*

**Sources.** The whole effect is three small functions plus a palette convention:
- `lens_flares_for()` — spikes + ghost chain: `niv-lr/src/noctis-0.cpp:2990-3071`; original `niv-plus/source/NOCTIS-0.CPP:3639-3706` (adds `lens_flare_mode`)
- `white_globe()` / `white_sun()` — halo disc: `noctis-0.cpp:2761-2863`
- `fline()`/`stick()` additive line rasteriser: `noctis-0.cpp:1551-1663`; original asm `TDPOLYGS.H:1578-1620`
- `psmooth_grays()` 4×4 blur (the "bloom"): `noctis-0.cpp:286-350`

**The blending model.** 256-colour palette = 4 banks of 64; low 6 bits = intensity along a tint ramp, high 2 bits = which ramp (0–63 vehicle/HUD, 64–127 cosmos/sky, 128–191 stars/moons, 192–255 planets). All glow drawing is **saturating integer add on the low 6 bits, preserving bank bits**:
```asm
mov al, es:[di-1]  /  and al, 0x3F  /  add al, colore  /  cmp al, 62  /  jb flow  /  mov al, 62
```

## 1. The ghost chain

No sprite table. The ghosts are **3 shrunken copies of each 8th starburst spoke, mirrored through screen centre**:
```c
if (on_hud && !(c % 8)) {
    dx /= 10;  dy /= 10;
    xr = xs * -0.1;  yr = ys * -0.1;
    for (r = 0; r < 3; r++) { fline(xr-dx, yr-dy, xr+dx, yr+dy);
                              dx *= 4; dy *= 4;  xr *= 3; yr *= 3; }
}
```

| n | position (× light screen pos) | half-size (× parent spoke) | blend |
|---|---|---|---|
| 0 | −0.1 | 0.1 | add +8/px, clamp 62 |
| 1 | −0.3 | 0.4 | 〃 |
| 2 | −0.9 | 1.6 | 〃 |

Ghosts are drawn per qualifying spoke, oriented like the parent — so each ghost is itself a tiny starburst. Gated by `on_hud` (in-fiction: reflections in the suit visor). NIV+ exposes `lens_flare_mode` ∈ {0 visor-only, 1 always, −1 never}.

## 2. The starburst

Procedural fan of 2D additive lines — no sprites, no rotation:
```c
for (c = 0; c < 180; c += added) {
    dx = lft_cos[c]*k*l;  dy = lft_sin[c]*k*l;
    fline(xs-dx, ys-dy, xs+dx, ys+dy);   // symmetric → full 360°
    l *= u;  if (l > 3 || l < 1) u = 1/u;   // u starts 1.5
}
```
- **Angles** screen-fixed (spokes never rotate with view or roll); spoke count = 2·⌈180/added⌉.
- **Length law**: half-length `k·l`; `l` zig-zags geometrically ×1.5 between 1 and 3: 1, 1.5, 2.25, 3.375, 2.25, 1.5, 1, 0.667 — a deterministic jagged star, **stable frame-to-frame (no shimmer)**.
- **`k` (size)**: callers pass `interval = 10*star_ray/dist` → `k = dist/ray` px. Flare window 5–1000 radii, so k ∈ [5..1000] px, screen-clipped. Cabin lamps pass `interval = −5e5` → fixed world size, true perspective.
- **Spoke count law**: `added = 1 + 0.001*dist` (space), `1 + 0.002*dist` (surface). Far away `added > 180` → **only c=0 survives: a single long horizontal streak** — the iconic anamorphic streak of distant suns falls out of the loop bounds **for free**.
- **Falloff**: none along a line — constant +8; radial falloff emerges because spokes overlap and saturate near centre.

## 3. Halo / glare disc

`white_globe` (space, half-res 2×2 blocks) / `white_sun` (surface, 1×1):
```c
mag_factor /= rz;  if (mag_factor > 2.99) mag_factor = 2.99;   // radius cap
mag = mag_factor*100 + 1.5;      // px radius, max ~301
fgm = fgm_factor * mag;          // full-bright core radius
ise = 0x3F / (mag - fgm);
if (zz < magsq) { pix = (zz > fgmsq) ? 0x3F - (sqrt(zz)-fgm)*ise : 0x3F;   // LINEAR falloff
                  pix += target[p];  if (pix > 0x3F) pix = 0x3F; }         // saturating add
```
- Radius `r = clamp(K/z, 0.01, 2.99)*100 + 1.5` px; `K = 3*star_ray` (space), `4*nray1` (surface w/ atmosphere, fgm=0 → all-soft blob), `3*nray1` (airless, fgm=0.5 → hard core + skirt). Space core fraction **0.3**; companions get randomized `0.15 − rand*0.3`.
- Falloff **linear in radius** (not gaussian). `ya += 1.2` per row = aspect correction so it's round on a 4:3 CRT.
- **Bloom**: after halo+spikes, one `psmooth_grays_ex` 4×4 box average over the whole space view, then `mask_pixels(+64)` shifts into the tinted sky bank. Order: **glow → blur → tint-bank shift → star disc on top**.

## 4. Occlusion & fading

**One single-pixel framebuffer sample** at the light's screen position, testing the palette *bank*:
```c
case 1: if (adapted[...] < 64) exit;                  // cabin lamps
case 2: if (adapted[...] < 64 || > 127) exit;         // surface sun: pixel must be SKY bank
```
Surface renderer draws sky+sun first, then terrain/objects, then calls the flare with `condition=2` — so a mountain kills the flare. Binary, **no temporal smoothing — it pops**. What Noctis has instead: palette targets walk ±1/frame (colour transitions never pop); flight mode replaces clear with `pfade(180,8)` so motion pops smear over ~8 frames. Weather gates: no flare at night, `rainy >= 1.2`.

## 5. Screen-space math & off-screen behaviour

```c
rx = xx*opt_pcosbeta + zz*opt_psinbeta;   // pcos/psin include ×dpp (focal 210, FOV ≈74°)
if (rz > 1) { xs = rx/rz + xshift;  ys = ry/rz + yshift;
              if (xs > -150 && ys > -90 && xs < 160 && ys < 90) { ... } }
```
- `rz > 1` guard: behind/at camera → whole effect vanishes. **No mirrored-ghost bug possible** because ghost positions derive from the guarded `xs,ys`.
- The centre must be on screen; spokes clipped per-line to screen inset 10 px. When the light's centre crosses the edge the whole flare disappears in one frame — a hard pop, **the one behaviour not worth copying verbatim**.

## 6. Per-class variation

- **Suppression**: no flare for classes 5, 6, 10; class 11 (pulsar) flares only while `gl_start < 90` — 25% duty rotating flash, light RGB ×5 during flash.
- **Colour**: flares carry no colour — the *palette banks* do. Sky bank rebuilt black→class-midtone→class-bright→(64,70,76), with wash-out `satur = 6.4*dsd/ray` cap 44 when close.
- **Geometry**: spoke length `k = dist/ray`, count `added = 1+0.001*dist`; radius differences come free from `class_ray` (250..30000).

## 7. Cheap tricks (disproportionate payoff)

1. **Palette-bank additive blend** — hue from the top 2 bits of the *destination*: the flare tints itself with whatever it crosses. One adder, zero multipliers.
2. **1-pixel occlusion test** by bank range — visibility for the cost of one read.
3. **The l = 1↔3 ×1.5 zig-zag** — jagged organic starburst from 3 constants, deterministic (no twinkle noise, no flicker).
4. **Ghost chain = same line primitive**, offsets −0.1/−0.3/−0.9, sizes ×0.1/×0.4/×1.6 — a lens-reflection chain in 6 lines.
5. **Degenerate far case**: `added > 180` collapses the fan to a single horizontal streak — distance LOD for free.
6. **One 4×4 box blur** = the entire bloom budget.
7. **±1/frame palette walk** = temporal smoothing without per-effect state.
8. **1.2 y-step** in the halo = aspect-corrected round glow.

## What we should steal — ranked, with FPGA verdicts

| # | Technique | Verdict | How / frozen constants |
|---|---|---|---|
| 1 | **Ghost chain** (−0.1,−0.3,−0.9 × light pos; sizes 1:4:16) | **Directly portable** | 3 additive sprites per bright light. Q8: pos = `(−26,−77,−230)/256 × (sx,sy)`; size = base/8, base/2, base×2; tint = light tint at ¼ alpha. Gate on the same visibility bit. |
| 2 | **Additive saturating blend, tint-from-table** | **Directly portable** | Saturating add in RGB565 per channel reproduces the 6-bit clamp. Flare sprites white-on-black CLUT × per-light tint register. |
| 3 | **Halo: r = K/z clamped, flat core 30%, linear falloff** | **Portable if simplified** | Don't rasterise — bake ONE radial sprite (flat core at 30% radius, linear ramp to 0) as 64×64 CLUT and scale. `r = clamp((Kray·recip(z))>>s, r_min, r_max)`, cap ≈ 0.94×half-height. Two variants: soft (core=0, atmosphere) and hard (core=0.5, airless). |
| 4 | **Starburst fan with l-zig-zag** | **Portable if simplified** | No line engine: freeze the l-sequence into 2–3 pre-rendered starburst sprites (4-spoke, 12-spoke, 1-streak); spoke ratios {16,24,36,54,36,24,16,10}/16 in Q4.4. Select sprite+scale by distance bucket via an 8–16 entry LUT (`added = 1 + d/1024` as a shift). Screen-fixed rotation, like the original. |
| 5 | **Far collapse to one horizontal streak** | **Directly portable** | Last LUT bucket = 1-streak sprite, wide and thin. **This is the signature Noctis look at range — do not skip.** |
| 6 | **1-sample occlusion** | **Portable if simplified** | Latch the compositor's layer/priority ID at the light's screen coord during the *previous* frame's scanout — one register, one compare. **Add what Noctis lacks**: a 4-bit up/down counter (±1/frame) driving flare alpha → 8–15-frame fade, kills the pop. |
| 7 | **Behind-camera & on-screen gating** | **Directly portable** | Reject if `rz ≤ 1` and centre off-screen. **Improvement**: fade alpha over the outer 16-px border instead of the hard cut — removes their one ugly pop. |
| 8 | **4×4 box-blur bloom of the glow layer** | **Portable if simplified** | Render halo+spikes into a quarter-res (80×60) glow plane, one 4×4 (or two 2×2) box passes with shifts only, upsample, additive composite. Their half-res 2×2-block halo is itself precedent. |
| 9 | **Per-class flare identity** | **Directly portable** | Frozen table by class: {tint RGB565, flare-enable bit, pulsar-flash bit}. Pulsar: enable = phase < 90/360, tint ×5 saturating during flash. Suppressing flares for the three "dead" classes is part of why bright stars feel special. |
| 10 | **pfade motion trails** | **Not portable as-is** | Whole-frame read-modify-write fights a sprite compositor. Approximate only on the low-res glow plane if wanted. |
| 11 | **Live Bresenham line fan** | **Not portable** | Unbounded per-frame pixel writes with per-pixel RMW. The baked-sprite version (#4) preserves the look at fixed cost. |

**Suggested frozen constant block**: `GHOST_POS = {-26,-77,-230}/256`, `GHOST_SIZE = {2,8,32}/16 of spoke`, `HALO_CORE = {0,5,8}/16` (atmo/space/airless), `HALO_MAX_R = 0.94·half_height`, `SPOKE_LEN_SEQ = {16,24,36,54,36,24,16,10}/16`, `SPOKE_ADD ≈ alpha 32/255 additive`, `FADE_CTR = 4-bit ±1 @60Hz`, `DIST_LUT[8]: (spokes, scale) = (12,s)…(4,s)…(1,streak)`.

**The heart of the look, in one sentence:** a saturating-add starburst whose spokes zig-zag in length, three mirrored mini-copies of it along the lens axis, a linear-falloff halo, all box-blurred once and tinted by the star's class — every piece of which maps onto frozen-table additive sprites plus one bounded low-res blur pass.
