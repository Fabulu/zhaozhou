# SPEC v1 — Owner Direction 29: a sun in every clip, visibly lit, distinct colours

## Objective
Give every one of the 21 sun-less bank clips a positioned point source ("sun")
with the approved additive emission ON, so the creature is VISIBLY lit by it at
native 384x240, with distinct lighting moods across the bank. Publish tonight.

## Acceptance (owner's words: "the creature's actually visibly lit")
1. Every clip has a sun; the creature is visibly lit by it at 384x240.
2. Colours differ meaningfully between videos.
3. No sun reads as from below or behind. Suns are ABOVE and to one side.
4. Form and pigment still read; nothing neon; nothing washed flat.
5. Animation/geometry/rig/pigment untouched (proof: suns-off CRC == prior
   baseline table; probe green).
6. Outgoing bank preserved as Archive Generation Eighteen; both CSS selector
   families extended; MAX_ARCHIVE_GENERATIONS 17 -> 18.

## Design
- `ZixxSunSpec` per clip: named mm offsets from the tracked instance
  (above + one side), radii with the whole body INSIDE the inner radius
  (attenuation = 1 -> pure directional lambert; no invisible-radius trap by
  construction), per-channel mult gain (complement suppressed) + additive
  emission (the visibility workhorse under the Cool Cross clamp headroom).
- SceneSubject gains `const ZixxSunSpec* sun`; compose installs it as a
  1-source point array with g_creature_additive_light scoped ON (gate kept).
- ZIXX_SUNS=off env kill-switch = the revert path demo (byte-identity run).
- moving-light + additive subjects keep their four-source rigs untouched.
- No marker orbs: suns sit out of frame; there is no orb to lie.

## Verification plan (bounded)
- Suns-off CRC across all 22 == RUN-2144 baseline table (animation untouched).
- zixx-probe green.
- By eye at native + 3x on key frames per subject: visibly lit, above/side,
  form and pigment read, no neon, no all-three-channel clamp flood.
- 22 renders complete (frame counts == baseline table).

## Publish
Archive gen eighteen, rewrite clip notes, tovideo.py encode 21 clips,
assemble.py, merge remote mains (no force push), probe rerun,
deploy.ps1 -Project upheaval -Branch main, verify 200/media/archive/noindex.
