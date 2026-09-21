# P22 findings 01 — where the dot radii come from (read, not guessed)

## Item 1: the dots are FULLY SCREEN-SPACE today. Not scaled at all.

- `mana_fold()` (manafold_fx.h ~3788-3826) picks the halo radius by hash:
  `halo = kMoteHaloRPxMin + hash%(Max-Min+1)` = **7..10 px** (manafold_art.h:4432).
- Two pushes, both `mana_push` (so `ManaSplat::line == false`):
  - halo: `r=halo`, ramp, gain `kMoteHaloGainPm*visible/1000`, surface_fade=true
  - core: `r=halo*kMoteCoreOfHaloPm/1000` with kMoteCoreOfHaloPm=**1600**
    (manafold_art.h:4468) -> **11..16 px**, opaque+soft (the heart is BIGGER
    than the additive halo; pass 8's order fix).
- Renderer (zhao_reel.cpp:3289 pre-pass, :3878 post-pass):
  `r = ms.line ? u02::mana_line_r_px(ms.r_px, primary_radius_q8) : ms.r_px;`
  So a non-line splat takes its authored pixel radius verbatim at every
  distance. **Confirmed: constant screen size, zero distance dependence.**

## Pass 19's distance law, to reuse

`u02::mana_line_r_px(r_px, projected_radius_q8)` (manafold_fx.h ~1869):
  r' = round(r * proj_q8 / (full_px*256)), >=1, <=r ; exact legacy at proj>=full.
- measure = `primary_radius_q8`, the cel ink's own operand (zhao_reel.cpp:3177),
  fed to `cel_main_ink_width`.
- reference = `kManaLineFullRadiusPx = 360`, matching `kCelInkCloseRadiusQ8 =
  360*256` (zhao_reel.cpp:3142).
- knobs: `ZHAO_U02_MANA_LINE_SCALE=distance|legacy`, `..._MANA_LINE_FULL_PX`.

So the dot law reuses the SAME measure and the SAME 360 px reference point.
A pure linear law at Drift's 128 px would put halo 7..10 -> 2..4 px and core
11..16 -> 4..6 px. That risks "turns to single pixels", so the dot law gets a
STRENGTH in per-mille plus a minimum radius, laddered by eye.
