# Task Log: RUN-20260903-2017 - Direction 27: prove the coloured lights work, then make them read

**Created:** 2026-09-03 20:17 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-2017-zixxtrixx-colour-light-repair/

---

## Objective

Prove each of the four moving sources illuminates the creature (marker==light), find why the colours are invisible, then make blue/orange/green obvious with visible mixing at native 384x240, and publish.

---

## Progress Timeline

### 2026-09-03 20:17 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-2017
- Created working directory
- Initial context: Direction 27 read; shade path traced: quant_shade per-channel clamp at 1.0 + mean-thresholded toon ramp are prime suspects; celmain uses smooth-toon path, not g_cel_bands

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-09-03 20:35 — Phase 1: static trace + calibrated instrument + prediction

**Shade path traced** (creature_sim.cpp): PointShade3 sums gain*lambert*atten
per channel -> creature_light(points_active) adds to amb+key+fill and
quant_shade CLAMPS EACH CHANNEL AT 1.0 -> Gouraud corners -> rasteriser
apply_toon_ramp (celmain = smooth-toon path; mean-thresholded 3 bands,
ratio-preserving q/m scale) -> texel multiply -> sat_u8. Colour survives as
CHANNEL RATIO through both quantisers; it dies at the quant_shade ceiling.

**Instrument calibrated**: lane build at 81fbe24c with env unset reproduces
published moving-light CRC 0x756E0BFF exactly (600f).

**Committed diagnostic**: ZIXX_ML_SOLO=none|warm|blue|orange|green +
ZIXX_ML_BOOST=n, default off = byte-identical.

**Prediction from the real constants** (to be checked against solo renders,
never shipped as values): warm lamp gains 2.25/1.70/1.10 with inner radius
2300mm put ALL THREE channels over the 1.0 ceiling across its whole pool ->
flat white, band 2. Under it a coloured source changes ZERO pixels. The warm
lamp's own warmth is also clamped away (reads white, not warm). Coloured
pools only read in warm shadow, in the dark band, and their orbits never
enter their own inner radius (att <= ~0.4 at closest approach), so the
maximum shipped colour event is a faint dark-band tint. This reproduces the
owner's report exactly: one light seems to work, no colours, no mixes.

## 2026-09-03 20:50 — Phase 1 empirical (interim)

Solo renders against the all-gains-zero dark plate (header-verified reader):
* WARM solo: peak 3353 changed px; pool mean lifts (39,63,57)->(98,152,116) —
  a strong HUE-NEUTRAL brightening (its warm tint is above the clamp; final
  colour is all pigment). This is the one visible light.
* BLUE solo: contributes on 292/600 frames (peak 1471 px) — the source
  mechanically WORKS — but the peak colour event is (32,64,55)->(28,60,65),
  +10 blue on ~60-luminance pixels. Invisible at 240p. Matches prediction:
  orbit never enters its inner radius, att <=~0.4, and it only reads in the
  dark band.

Phase 2 design sketch (to be authored by eye against renders, knobs only):
warm gains cut below whiteout so its own warmth reads and it stops erasing
the scene's shading range; coloured paths pulled closer to the body so each
source actually enters attenuation ~1 at approach; inner radii up (350 ->
~900) and outer up (~1500 -> ~2400) so pools are broad enough to see and to
overlap; gains re-pointed for hue purity (dominant channel ~1.6, secondaries
low) since a clamped DOMINANT channel keeps hue while three clamped channels
are white. All existing named constants; no new sources.

## 2026-09-03 21:15 — PHASE 1 VERDICT (committed evidence in evidence/)

Eight controlled renders (dark plate + each source solo at 1x and the three
colours at 8x), all against the calibrated binary that reproduces the
published CRC. Every frame read header-verified.

**1. Every source illuminates.** Changed-pixel counts vs the dark plate:
warm 600/600 frames active (peak 3356 px), orange 600/600 (peak 2609),
green 511/600 (peak 731), blue 292/600 (peak 1471). "It doesn't work at all"
is FALSE — no divergent-marker bug this time.

**2. Marker and light share one world position.** Structurally: both the
depth-tested orbs and the compositor consume the same cr_ctx.moving_sources
array, sampled once per frame (zhao_reel.cpp). Empirically: blue8's pool
centroid sits directly under the orb whenever the orb crosses the body
(f189 orb(156,82) pool(145,106); f486 orb(147,81) pool(145,106)); the pool
is body-bounded so corr(orb_x,pool_x)=0.63 with saturation at body edges —
consistent with one position, inconsistent with a divergence.

**3. Why the owner sees nothing.** The previous pass's transport claim is
TRUE (pools sum per channel before the quantisers, hue survives both as
channel ratio) but the VALUES kill the read:
* quant_shade clamps each channel at 1.0. Warm gains 2.25/1.70/1.10 with
  inner radius 2300mm put ALL THREE channels at the ceiling across its whole
  pool -> the warm lamp reads as a HUE-NEUTRAL brightener (pool colour is
  pure pigment) and it floods most of the animal, most of the time. That is
  the owner's "just the one" light.
* The coloured orbits never enter their own 350mm inner radius (closest
  approach ~1000-1200mm against outer 1500/1700 -> attenuation <=~0.4), so
  peak coloured events are +10..35 counts on ~60-luminance dark-band pixels.
  Blue's peak event: (32,64,55)->(28,60,65). Invisible at 240p. The
  invisible-crayon-grain failure, in light gains.
* At 8x boost every colour is plainly visible (blue8 turns the dorsal pink
  violet under the orb) — confirming the chain is healthy and only the
  authored values are wrong.

Phase 2 therefore: no renderer bug fix needed; re-author the reel's named
knobs (warm tamed below whiteout, coloured paths closer, radii broader,
gains purer/stronger), by eye, at native resolution.
