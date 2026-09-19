# Q001 trick-360-design
max_tokens: 14000
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Design (do NOT write the final patch yet) how to extend the creature's "Trick" clip for owner direction item 9.

Owner's words: "Trick makes a 180° turn and 180's back. I think it'd be cool if we did the 180, paused a bit like now, then did a 360, overshot a little, and corrected back."

Ratified architecture (binding): keep the version-17 pure-X half-turn plant (the creature does a headstand on its antenna) and the +90° planted face yaw correction. Keep the current pause. Then add a PLANTED YAW show-off: one full C2-continuous revolution about the vertical support axis (the antenna stays planted in the dirt the whole time), a slight named overshoot past 360, a C2 correction back to exactly identity, then the EXISTING righting/recovery. Author the spin as unwrapped per-mille progress (0..1000 = one revolution, overshoot > 1000), converting to angle16 (65536 = 360°) only when building the quaternion — this avoids angle wrap bugs. No camera chase. The contact window and support ownership must stay valid (antenna support must stay planted). Clip length may grow (kTrickKeys) if needed; every timing and amplitude must be a named editable constant.

Note: the source is being edited concurrently (root height / pivot about the support centre). Design against what is shown; flag assumptions.

## Questions
1. Summarise the current Trick timeline key by key range (approach, flip, plant, pause, righting, home) with the constants that define each, citing lines.
2. Where exactly in build_trick is the per-key root orientation composed? Which quaternion multiplication order? Where would a yaw about the WORLD vertical support axis have to be inserted so the planted antenna stays put (rotation about the support point, not about the root)?
3. Propose the new timeline: named constants (key ranges, overshoot per-mille, pause lengths), whether kTrickKeys/kTrickLiftKey/kTrickHomeKey must move, and how the probe contact window [kTrickPlantKey, kTrickLiftKey) relates to the spin.
4. Propose the C2 easing for 0 -> (1000+overshoot) -> 1000 progress (e.g. quintic segments), with exact formulas, and show that velocity and acceleration are zero at the joins.
5. Give pseudo-code (C++-like, using the helpers visible in the input) for the spin insertion. Keep it short.
6. List risks: face read during the spin, antenna support ownership, loop seam, interaction with kTrickShowoffYawA16/kTrickFaceYawA16.

## Inputs
tools/reel/manafold_art.h:2815-2880
tools/reel/manafold_clips.h:3204-3305
