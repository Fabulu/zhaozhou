# STAR GAMUT AND LENS FLARE ARCHITECTURE ADDENDUM v1

**Status:** RATIFIED v1 (2026-08-15) — from run RUN-20260815-2307 (evidence: three Noctis IV recons N1/N2/N3 in `runs/CLAUDE-RUNS/RUN-20260815-2307-noctis-suns-and-flares/`; starfield hash ground truth three-way verified in the external noctis harness, imported as `tests/golden/starfield/`). This file is the verbatim in-repo copy of that run's ratified ADDENDUM; version both together on any semantic change.
**Implementation clarification (2026-08-16, recorded when the §13 anchors were first computed):** the D3 ramp control points P1/P2/P3 all use the plain ×4 expansion into the s16 pre-clamp domain (the `S00 ramp[32] = (254,224,136)` anchor is only reproducible that way); the D2 expansion `c8 = (c6<<2)|(c6>>4)` applies where a class colour is used directly as an 8-bit display colour (tints, glints), never inside the ramp domain.
**Implementation clarification 2 (2026-08-16, flare sprite bake):** §5's "1-px additive lines value 32/255" taken literally (draw 32, mean 8× downsample) yields peak spoke brightness **4/255** — an invisible flare, contradicting both the source effect (Noctis draws +8/63 ≈ 32/255 per FULL-RES pixel) and this spec's own thesis. The 512² canvas is 8× supersampling, so the mean downsample divides a hairline's energy by 8. The consistent reading, implemented: **bake lines draw saturating 255; "32/255" is the resulting DOWNSAMPLED arm brightness** (255/8 ≈ 32 for a 1-px spoke), with spoke crossings near the core saturating toward 255 before the mean. Every other §5 constant is unchanged.
**Authority:** owns star/sun/lens-flare/starfield semantics. Charter §8 pass 6 admits all celestial quads; §16 Mirror Gate admits the flare mode (precedent: `radial_decay`, sky_and_beams §3). ABI reservation **`0x0320..0x032F`**.
**Numeric law:** every formula is integer/fixed-point per `spec/qformats.md`. No host floats. Where a Noctis float constant is adapted, original and adapted values are both stated.

**Thesis:** *colour never exists at draw time for celestial light — intensity is drawn, and a per-frame ARM-rebuilt palette colourises it* — except that on Zhaozhou "the palette" is a 64-entry CLUT page, not the DAC, so the discipline costs 256 B of upload per frame and **zero fabric**.

## 1. CLUT ramp discipline (D1)

**Adopt intensity×ramp as a *texture/material* discipline, not a framebuffer discipline.** The frozen tile store (24-bit working colour + 8-bit tag) already gives what Noctis's palette bought; re-introducing an indexed framebuffer would be regression. The *asset-side* insight ports intact onto CLUT8.

- **Ramp bank = a CLUT8 palette page**, 64 entries, ARM-rebuilt **every frame** per active near star (≤2). Upload ≤512 B/frame + palette-page invalidate; resource-epoch ordering guarantees it lands before pass 6.
- **Material recipes:** `star_disc_masked` (CLUT8 nearest+mips, alpha-test index 0, Z-test on/Z-write off, glow-tag write); `star_halo_additive` (CLUT8 nearest+mips, `dst = sat(dst+src)`, Z-test on/Z-write off, glow-tag write). **Nearest mandatory** — bilinear must never touch a palette.
- **Effect-tag convention (frozen):** `tag = (channel << 6) | strength`, `GLOW = 0b01`, strength = source texel's CLUT intensity (0..63) — Noctis's palette byte reborn as the effect tag.
- **Resolve survival:** ordered dither applies to RGB565 only — **the tag byte is never dithered**. The flare probe reads the latched tag, so the flare system survives resolve untouched.

## 2. Star gamut table (D2)

**Adopt Noctis's 12 classes verbatim as compiled spec-frozen defaults.** The 120× radius span and the three dead classes are what make bright stars feel special. RGB are original VGA 6-bit; expansion `c8 = (c6<<2)|(c6>>4)`. `dfs` U2.6. `SPIN_K = 55` angle16 units/tick per Noctis degree/frame (65536/360 ÷ (60/18.2) = 55.16 → 55, error 0.4%). `radius_milli = class_ray + (h1 mod class_rayvar)`.

| # | Name | class_rgb | undertone | class_ray | rayvar | dfs | smooth | spin | flare |
|---|---|---|---|---|---|---|---|---|---|
| S00 | Yellow star | 63,58,40 | 64,54,28 | 5000 | 2000 | 64 | 2 | 0 | Y |
| S01 | Blue giant | 30,50,63 | 36,50,64 | 15000 | 10000 | 96 | 2 | 0 | Y |
| S02 | White dwarf | 63,63,63 | 24,32,48 | 300 | 200 | 32 | 5 | 55·(1+h mod 4) | Y |
| S03 | Red giant | 63,30,20 | 64,24,12 | 20000 | 15000 | 51 | 2 | 0 | Y |
| S04 | Orange giant | 63,55,32 | 64,40,32 | 15000 | 5000 | 77 | 2 | 0 | Y |
| S05 | Brown dwarf | 32,16,10 | 28,20,12 | 1000 | 1000 | 6 | 2 | 0 | **N** |
| S06 | Grey giant | 32,28,24 | 32,32,32 | 3000 | 3000 | 6 | 2 | 0 | **N** |
| S07 | Blue dwarf | 10,20,63 | 32,44,64 | 2000 | 500 | 26 | 5 | 55·(1+h mod 12) | Y |
| S08 | Multiple | 63,32,16 | 64,60,32 | 4000 | 5000 | 58 | 2 | 0 | Y |
| S09 | Infant star | 48,32,63 | per-identity | 1500 | 10000 | 83 | 2 | 0 | Y |
| S10 | Runaway | 40,10,10 | 32,26,22 | 30000 | 1000 | 32 | 2 | 0 | **N** |
| S11 | Pulsar | 0,63,63 | 36,48,64 | 250 | 10 | 13 | 5 | 55·(1+h mod 30) | duty |

- **Pulsar duty (frozen):** active ⟺ `spin_phase < 0x4000` (exact quarter turn; original `gl_start < 90`). `pulse_gain = 160` U3.5 (=5.0, original ×5) during flash, 32 (=1.0) off.
- **Infant undertone:** `under6[c] = 24 + ((h3 >> (5·c)) & 31)`.
- **Non-compact rotation:** Noctis used wall-clock — **dropped** (host time forbidden); disc spin is carried by the CLUT boil.
- **dfs** feeds the ratified surface-sun grading (sky_and_beams §1.1) — free per-class mood.

## 3. Star disc (D3)

**One screen-space masked billboard quad + a static ARM-generated CLUT8 texture; all animation is palette work.** Rejected: runtime QT-VR splat (framebuffer-poking, bypasses the tile pipeline) and Forge sphere mesh (Noctis proved zero 3D geometry is needed). The QT-VR trick survives, moved to **ARM bake time**.

- **Geometry:** `DRAW_PROCEDURAL` forge kind `star_quad` — params `{screen_x, screen_y S12.8; radius_px S12.8; depth invw24; texture_id; palette_id; mode disc|halo}`. Pass 6, Z-test on, Z-write off, `depth = STAR_DEPTH` (sky-prefill far + 1 → beats the sky backdrop, loses to real surfaces). Halo submitted before disc.
- **Texture (`starface`, 128×128 CLUT8 + mips ≈ 21.8 KB), bake law:**
  1. Cylindrical grid 256×64: `g[x][y] = (noise2_hash(x, y, texture_seed, 0) >> 26) & 0x3E` — **PCG per qformats §7.5, ratified over Noctis's middle-square** (one hash discipline; character comes from the smoothing).
  2. Box-smooth 3×3 (wrap x / clamp y): **2 passes**, **5 for compact classes** S02/S07/S11.
  3. Orthographic resample through the compiled QT-VR offset map into the 120-half-texel disc; outside → index 0.
  4. Index 0 transparent; intensity 1..63.
- **Granulation = CLUT rotation, zero pixels touched, zero fabric** (cheaper than Noctis's per-texel increment, which would be render-to-texture here — refused):
  ```
  rot      = (tick / 3) mod 63                 # BOIL_DIV 3 → 20 steps/s (original 1/frame @18.2)
  SATUR    = min(63, (12·d) / r)               # distance washout (original 12·dsd/ray)
  pal_d[e] = ramp[ max( 1 + ((e−1+rot) mod 63), SATUR ) ],  e = 1..63;  pal_d[0] = transparent
  ```
- **Ramp build (frozen, s16 pre-clamp domain):** `P0 = (0,0,0)`, `P1 = undertone8`, `P2 = class8`, `P3 = (256,280,304)` (6-bit 64,70,76 ×4 — deliberate early per-channel saturation whitens the top). Segments `[0..24) P0→P1`, `[24..40) P1→P2`, `[40..64) P2→P3`; within `[a,a+n)`: `ramp[i] = base + round_half_up((tgt−base)·(i−a), n)`, clamp [0,255]. **Slew ±1/tick per channel** toward targets — palette changes never pop.
- **Disc clamp** `DISC_RMAX = 112 px` (original 132 on 200 lines).

## 4. Corona / halo (D4)

**One baked radial CLUT8 sprite per core-fraction variant, scaled — no per-pixel analytic evaluation anywhere** (the r²→LUT question is moot: the LUT *is* the texture).

Bake law, 128×128, half-texel radius `R_h = 120`:
```
rr    = isqrt((2x−127)² + (2y−127)²)
fgm_h = round_half_up(R_h·core16, 16)
k     = round_half_up(63<<16, R_h − fgm_h)
pix   = rr ≥ R_h ? 0 : rr ≤ fgm_h ? 63 : 63 − rescale_u((rr − fgm_h)·k, 16)   # LINEAR falloff
```

| Variant | core16 | fgm_h | k | Use (quad scale) |
|---|---|---|---|---|
| `halo_atmo` | 0/16 | 0 | 34406 | surface sun w/ atmosphere — pure glow ball (4×R) |
| `halo_space` | 5/16 | 38 | 50351 | space corona (3×R) |
| `halo_airless` | 8/16 | 60 | 68813 | airless surface sun — hard core + skirt (3×R) |

Material `star_halo_additive` through the **same ramp palette** (un-rotated, un-floored; `pal_h[0]` = black = additive identity) — the corona is automatically the star's colour fading to black. `HALO_RMAX = 225 px` Z60 solo / **160 px per Duo view**. Companion per-star randomised core **dropped** (one texture; recorded honestly).

## 5. Lens flare (D5) — a Mirror Gate mode

**The entire chain is frozen-table additive sprite splats into the low-res glow buffer, a bounded POST.COMPOSITE mode beside `radial_decay`.** Live line drawing refused (§26: unbounded per-pixel RMW ≈ a fragment program).

**Sprites** (asset compiler, goldens committed): `burst12` 64×64, `burst4` 64×64, `streak` 96×16. Canvas 512² (streak 768×128), spoke i of n at screen-fixed angle `i·180/n` (spokes never rotate — original behaviour), symmetric, half-length `L_i = R_canvas · SPOKE_LEN_SEQ[i mod 8]/54`, 1-px additive lines value 32/255 saturating, 8× box downsample.
`SPOKE_LEN_SEQ = {16,24,36,54,36,24,16,10}/16` — the ×1.5 zig-zag frozen as Q4.4 (original 1, 1.5, 2.25, 3.375, …; deterministic, no shimmer). `streak` = the single c=0 survivor: **the iconic anamorphic line**.

**Per-light law** (≤2 lights/view):
```
k      = clamp(d/r, 5, 384)                      # spoke half-length px (original window 5..1000 radii)
b      = clamp(floor(log2(d/r)) − 2, 0, 7)       # distance bucket
sprite = b ≤ 3 ? burst12 : b ≤ 6 ? burst4 : streak    # adapted from added = 1 + 0.001·dist
scale  = k >> 2                                  # glow buffer is ¼ res
```

| Splat | position (× light pos) | half-size | alpha |
|---|---|---|---|
| burst | +256/256 | k | 255 |
| ghost0 | **−26/256** | k·26/256 | 64 |
| ghost1 | **−77/256** | k·102/256 | 64 |
| ghost2 | **−230/256** | k·410/256 | 64 |

(Original −0.1/−0.3/−0.9 and ×0.1/×0.4/×1.6 → Q8.8; ghosts reuse the burst sprite — each ghost is itself a tiny starburst, as in the original.) `glow += rescale_u(sprite_u8·a, 8)` saturating u16; tint applies at the existing upscale-composite. Bound `flare_texels ≤ 16384`/view/frame.

**Occlusion — 1 byte, plus the temporal fade Noctis lacks:**
- **Probe:** POST.GATHER latches the effect-tag byte at each light's probe pixel during its existing sweep (≤4 comparators + 4 byte registers). Visible ⟺ `(tag >> 6) == GLOW`. Reads a latch, never the framebuffer.
- **Fade counter:** per slot u4, ±1/frame toward (visible ∧ on-window ∧ in-front) ? 15 : 0; `fade_alpha = ctr·17`. 15-frame/250 ms fade — **never pops** (original: binary, popped).
- **Border fade** (replaces the hard off-screen cut): `border_alpha = min(255, clamp(edge_dist,0,16)·16)`.
- **Behind camera:** `w ≤ wmin` forces target 0 — decays through the fade; no mirrored-ghost bug (positions derive from guarded screen coords).
- **Gating:** class flare-enable bit, window `5r ≤ d ≤ 1000r`, weather/night flags (ARM omits the light).

## 6. Distance LOD ladder (D6)

| Projected disc radius | Representation |
|---|---|
| ≥ 6 px | textured disc + corona + flare |
| 1.5–6 px | corona sprite + flare (the core reads as the star) |
| < 1.5 px, d ≤ 1550r | **far glint with minimum-brightness clamp**: `intensity6 = 48 + clamp((1600r − d)/(100r), 0, 15)` — never dimmer than 75% (original `0x30 + …`) |
| beyond | procedural starfield point only |

§9 hysteresis + 15-tick minimum hold + 10% threshold hysteresis. Celestial work is **Measure-visible** (unlike the Measure-exempt sky), with the halo clamp as the degradation knob.

## 7. Procedural starfield (D7)

**Adopt the Noctis sector hash verbatim for existence/position; use our PCG for identity draws.** Argued both ways: PCG-everywhere gives uniformity, but the sector hash is pure int32, **already bit-exactly verified three ways** in `harness/`, and its fold arithmetic *is* the galaxy — the crushed-disc density gradient emerges from it. Identity draws were Borland `rand()` — ours to define; NIV-universe compatibility is a non-goal.

- **Existence/position: frozen by transliteration of `harness/oracle.c`** — 100,000-unit sectors, ≤1 star each, signed 32×32→64 multiply-fold (hi+lo, `& 0x1FFFF`, per-axis −50000); hash landing on the 50000 sentinel ⇒ no star. Golden vectors imported to `tests/golden/starfield/`; the ZRef transliteration must match byte-exactly before anything renders.
- **Identity schedule (frozen, replaces Borland rand):**
  ```
  h0 = noise2_hash(sx, sy, sz ^ GALAXY_SEED, 0)   h1 = noise2_hash(sx, sy, sz ^ GALAXY_SEED, 1)
  h2 = noise2_hash(h0, h1, GALAXY_SEED, 0)        h3 = noise2_hash(h0, h1, GALAXY_SEED, 1)
  class = CLASS_PICK[h0 >> 27];  radius_milli = class_ray + (h1 mod class_rayvar)
  spin_draw = h2;  texture_seed = h3
  ```
  `CLASS_PICK[32]`: S00×7, S01×2, S02×2, S03×4, S04×4, S05×4, S06×2, S07×2, S08×2, S09×1, S10×1, S11×1.
- **Rendering:** ±4 sectors (729; cap 2744/view), ARM-computed, emitted as one `DRAW_POPULATION` glint population → PART.SOFT, additive, 1–2 px, `depth = STAR_DEPTH`, glow-tag write. **Occlusion for free** via Z-test against the sky far constant (the Noctis index-range trick as a depth compare).
- **Magnitude:** `intensity6 = 63 − (rz >> 13)`, skip if ≤0.
- **Rarity gate (float removed):** `e = min(15, (|sx| + 30·|sy| + |sz|)/4000)`; skip if `(sx+sy+sz) & ((1<<e)−1)`. The Y×30 crush gives the milky-way disc.
- No twinkle (deliberate). Motion streaks not ported (§14).

## 8. Determinism + capture (D8)

Everything derives from `(GALAXY_SEED, sector coords, tick)`. Temporal state exists in exactly three places, all captured in a new `celestial_state` chunk: ramp slew control points+targets (56 B/ramp, ≤2), flare slots `{light_id, fade_ctr, latched_tag}`×4 (16 B), per-star `{class, identity, radius_milli, spin_phase}`×2 + GALAXY_SEED + camera sector (~32 B). Replay from any captured frame reproduces palettes, boil phase, flare alpha and pulsar duty bit-exactly.

## 9. Where each piece runs — and which refusal it avoids (D9)

| Piece | Runs | §26 refusal avoided |
|---|---|---|
| Class table, sprites, offset map | compile time | arbitrary compute kernels |
| Starface synthesis | ARM, once per star approach | render-to-texture |
| Ramp rebuild + slew + SATUR + **boil rotation** | ARM, per frame (≤512 B) | render-to-texture, second TMU, shaders — **zero fabric** |
| LOD / projection / flare k,b | ARM, **wide integers** (interstellar distances exceed fx16); only screen-space quantities cross to the FPGA | floating-point rasterisation |
| Disc + halo quads | FORGE.PRIM → normal tile pipeline, pass 6 | — (ordinary triangles) |
| Disc/halo shading | existing masked/additive recipes + CLUT8 nearest | general fragment shaders |
| Far glints + starfield | PART.SOFT glint rung | — |
| Flare splats, tint, fades | POST.COMPOSITE `flare_splat` (bounded) | shaders, unbounded line rasteriser |
| Occlusion probe latch | POST.GATHER (1 byte × 4 slots) | unrestricted render-to-texture |

**Frozen tile-store format untouched. No depth-buffer, TMU, or resolve change.**

## 10. Command surface

**`SetCelestials 0x0320`** — per view: flare light array (≤2: light_id, screen pos 2×S12.8, probe pixel u9/u8, k u16, bucket u3, tint RGB565, flags {enable, in_window, in_front}), `pulse_gain` U3.5, active ramp page ids. Reserved `0x0321..0x032F`. Discs/halos ride `DRAW_PROCEDURAL` (`star_quad`); starfield/glints ride `DRAW_POPULATION`; palettes ride ordinary resource upload.
**Form shapes:** `star { class, seed | explicit {tint, undertone, radius}, boil_div, ladder overrides }`, `flare { enable, tint_from_ramp, fade_frames }`, `starfield { galaxy_seed, cube_radius, rarity_divisor }`.

## 11. Ledger + cost + phase (D10)

**No new blocks; no ops.yml entries.** Notes amended on FORGE.PRIM (`star_quad`), RASTER.FRAGMENT (2 recipes + glow-tag law), TEXTURE.TMU/CACHE (hot palette page), PART.SOFT (glint rung serves stars), POST.COMPOSITE (`flare_splat`), POST.GATHER (probe latch), SW.CMDBUILD (celestial module).

**Budget lines:** `star_triangles ≤ 8`/view; `star_fragments ≤ 128K` Z60 / `≤ 64K` per Duo view; `flare_texels ≤ 16384`/view (≈¼ of `radial_decay`); `starfield_glints ≤ 2744`/view; VRAM ≤ 256 KB (<0.6% of the texture pool); palette upload ≤512 B/frame. Fabric: flare splat ≈0.5–1% ALM inside the 6% twod_post group; probe latch is noise.

**Phase:** hardware = **Phase 11** (ZH-044/ZH-046, with sky/beams). ARM/asset side + ZRef preview = **Phase 3+** — Wound Lab's night sky and the star-gallery capture exist long before RTL.

## 12. ZRef preview functions (D11)

`zref::star::identity / ramp / palette_disc / palette_halo / starface / corona_sprite / lod_select / glint_intensity`; `zref::flare::bake_sprites / emit / fade_step`; `zref::post::flare_splat`; `zref::sky::starfield` (must match imported harness goldens byte-exactly).

## 13. Test plan — hand-computable anchors

`star_ramp_anchor` (S00: `ramp[32] = (254,224,136)`) · `star_palette_boil` (tick 96 → rot 32; index 40 → ramp 9; at d=4r SATUR=48; at d≥5.25r all-white) · `corona_bake_anchor` (`halo_space` texel (105,63): rr=83 → pix=28; (64,63) → 63; rr=120 → 0) · `flare_ghost_anchor` (light (300,80), centre (192,120) → ghosts at (181,124), (160,132), (95,156)) · `flare_fade_occlusion` (255→0 over 15 ticks, no step >17) · `flare_border_fade` · `flare_far_streak` (d/r=600 → streak, k=384; d/r=4.5 → burst12, k=5) · `glint_min_brightness` (never below 48) · `pulsar_duty` (phase 15,400 on; 16,500 off) · `starfield_harness_equivalence` (byte-exact vs oracle.c goldens, **before any rendering test runs**) · `starfield_rarity_gate` · `star_lod_ladder` (no rung flip within 15-tick hold) · `star_gamut_sheet` (12 classes × 4 rungs × {near, washed}, CRC-locked, Duo included) · `capture_star_state_roundtrip` · `cost_assertions`.

## 14. Risks — honest

1. **We are sharper than Noctis, and that cuts both ways.** 384×240 > 320×200, but Noctis ran full-screen diffusion **twice per frame** plus a CRT — a large part of its hazy look. Our ordered dither is not that haze. Mitigations inside the law: mips, bake-time smoothing, quarter-res flares. A future implementer will want a full-frame diffusion pass — **that is a full-frame RMW the charter does not grant; refuse it**, or confine it to the low-res glow plane.
2. **No persistent framebuffer, no `pfade`.** Noctis's motion smear/warp trails/glow decay all came from fade-not-clear. Flares fade by counters instead. POST.ECHO could approximate later — it is **cut-order 1**; nothing here may depend on it.
3. **Charter-violation temptations, named so they stay dead:** live Bresenham flare fans; per-frame starface texel increments; sampling the framebuffer for occlusion; a "small" second sampler for halo gradients.
4. **fx16 cannot hold interstellar distances** (1550·r for a runaway giant ≈46,500 > 32,767). Star transform/LOD math is ARM-side wide-integer **by law**; pushing it into fx16 Field IR is an overflow trap.
5. **Palette-page ordering is load-bearing**: a palette missing its frame paints last frame's boil — invisible in stills, a determinism bug in captures. Capture tests are the tripwire.
6. **Nearest-sampled CLUT gradients**: `halo_space` at 225 px stretches 60 texels ≈3.75× — faint terracing expected; dither hides most. Escape hatch: a 256² corona bake (79 KB) — a texture swap, not a spec change.
7. **1-pixel occlusion probe**: thin occluders flicker it; the ±1 counter bounds the artefact to ≤17/255 alpha/frame. A multi-pixel probe would be scope creep toward readback.
8. **Verbatim class table = inherited art direction**; it is a compiled asset default — games may re-skin without touching this spec. These values are the anchors the tests hold.
