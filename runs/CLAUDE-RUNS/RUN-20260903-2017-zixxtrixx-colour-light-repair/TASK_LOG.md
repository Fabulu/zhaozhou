# Task Log: RUN-20260903-2017 - Direction 27: prove the coloured lights work, then make them read

**Created:** 2026-09-03 20:17 UTC+02:00
**Status:** Complete
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

## 2026-09-03 21:30 — Phase 2 v1 authored

Warm: gains 2.25/1.70/1.10 -> 1.05/0.72/0.30, inner 2300->1200, outer
7000->4500 (whiteout tamed; warmth below the ceiling on G/B).
Blue: orbit (2600,1500,h950)->(2050,1050,h800), inner 350->900, outer
1500->2600, gains (.18,.40,1.60)->(.08,.30,1.60).
Orange: orbit (2000,1250)->(1750,950), bob 350->300, inner 900, outer 2600,
gains (2.50,.85,.08)->(1.55,.55,.04).
Green: sweep 1900->1700, side -1000->-800, h 480->520, inner 900, outer
2600, gains (.25,1.35,.30)->(.10,1.50,.18).
Path-separation table says blue-orange near f325/450/575, orange-green at
f0/200/400, blue-green ~f150-250 — overlap events exist; judging by eye at
native next.

## 2026-09-03 21:50 — v1 looked at (native + 2x), v2 authored

v1 by eye: GREEN reads as a bright green pool riding the body; BLUE reads —
teal on the body green, violet-magenta where it crosses the dorsal pink
(honest mixing, lovely at the tail bend f60); the warm lamp brightens
without the old whiteout and the dorsal pink lights up hot pink under it.
ORANGE only reads at close kisses (f100 head, f225 blade tip) — its 3 laps
are phase-locked to green's 3 trips, so green upstages its every approach.

v2: orange closer (1550/820, h640) and stronger (1.90/0.60/0.04); green
3 -> 4 trips to break the phase lock; blue B 1.60 -> 1.70. New meeting
frames predicted: blue-orange f211, blue-green f225/f525, orange-green f170.

## 2026-09-03 22:10 — v2 looked at, v3 nudge

v2 by eye at native/2x: the bank comes alive. f300 carries green pool,
glowing pink dorsal, deep blue head and an orange-lit eye at once and the
creature still reads; f400/f525 blaze the eye amber under the orange pool
(the eye's red-rich pigments are orange's best canvas); f211's blue-orange
meeting lands on the near flank and turns the dorsal a hot magenta bloom —
genuine visible mixing. Physics note recorded: on the green body pigment a
pure red-heavy orange can only produce olive (multiplicative transport), so
orange must read via the pink stripe, the eye, and an amber body cast.
v3 (final nudge): orange R 1.90->2.00, G 0.60->0.70 (amber not olive).

## 2026-09-03 22:45 — v3 ACCEPTED; publication under way

v3 by eye: f400 = amber-lit eye, violet dorsal, bright green flank pool and
the blue orb in one still, and the creature is still itself. Whole-clip
contact sheet (50 tiles): coloured light alive in every tile, calm tiles
still exist, no broken frame. Seam 599->0 in family with the accepted bank
(2415 changed px vs the shipped bank's own 2124). Per-frame trace vs the v3
dark plate: blue-shifted pixels on 459/600 frames (peak 1216), warm/orange
on 600/600 (peak 2320), green on 596/600 (peak 2029) — versus the shipped
bank where blue's TOTAL footprint averaged 189 dark pixels/frame.

Archive Generation Seventeen preserved and pushed (Upheaval 3f9aeab; local
lane branch pushed as HEAD:main after a stale-local-main push rejection).
zhaozhou knobs committed fb9cba41. Poster moved 412 -> 400
(composition: all four colour reads in one still). Publication render: ONE
fresh explicit celmain/diagonal-cool-cross invocation of the 22 names into
a WIPED website scratch-reel. Probe building.

## 2026-09-03 23:05 — probe PASS at the accepted state

ZIXX PROBE: PASS (every key + midpoint, declared 3D contact,
balance/taunt/fall/impact/spring/jump/limit/overlap gates) from the same
build tree as the accepted v3 — the pass touches no animation and the
committed gate agrees. Publication render running.

## 2026-09-03 23:20 — determinism confirmed mid-render

The publication render's moving-light (fresh invocation, different directory,
same binary) reproduces the accepted v3 draft CRC 0x65A8D1E5 exactly.
Remaining subjects rendering.

## 2026-09-04 00:55 — CRC PROVEN, encoded, pushed

Publication render (ONE fresh explicit celmain/diagonal-cool-cross
invocation of the 22 names into a wiped scratch-reel) proven:
21 subjects byte-identical to the live bank (CRC-32C + frame count +
contiguity + per-frame 8+384*240*3 size), exactly one changed —
zixxtrixx-moving-light 0x756E0BFF -> 0x65A8D1E5 (600f), equal to the
accepted v3 draft from a different invocation (determinism).
evidence/publication-crc-final.txt. Only the changed clip re-encoded:
752,145 B VP9 crf16 4:4:4, decode-verified 600 frames, poster f400.
Upheaval de38080 pushed (HEAD:main). Deploying once.


## 2026-09-04 01:05 — CLOSED. Deployed and verified.

Deployed ONCE: deploy.ps1 -Project upheaval -Branch main (production, not a
preview). Verified live: page HTTP 200; served moving-light webm SHA256-equal
to the local 752,145 B encode (600 decoded frames); an Archive Generation
Seventeen file serves 200; exactly ONE noindex META tag (the second textual
occurrence is the footer prose, as on every accepted publish); the archive
tab lists Seventeen. Background jobs all stopped; process sweep clean
(no zhao/ffmpeg/wrangler/zixx).

Final SHAs: zhaozhou 7fd98bf8 (main, pushed), Upheaval de38080 (main, pushed).
