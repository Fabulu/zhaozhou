# SPEC v1 — Direction 30: additive is normal, one moving-light clip, suns calmed

## Binding direction
Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-30-2026-09-04.md

## Job 1 — retire the experimental framing
- One moving-light clip: the additive version becomes THE moving-light clip.
- Remove the duplicate entry and every "experimental"/"prototype" label from page copy and clip notes.
- Additive is the normal mode for all animations (default-on for the reel), gate `g_creature_additive_light` kept.
- Gate-off must still reproduce the pre-suns bank CRC-identically (re-prove).

## Job 2 — calm the suns
- Reference: Archive Generation Eighteen, compared side by side at native 384x240.
- Target: between Gen 18 and current, closer to Gen 18. Keep suns, per-clip colours, creature lit.
- No channel pegged across a large area; pigment and form dominate.
- Red-clip caveat: if a red clip still reads wrong after calming, dim the rig under it (one knob), do not push the sun.

## Do not touch
Animation, geometry, rig, pigment. Sun positions (50 m up / 22 m lateral) stay.

## Finish
Probe green + ink-mask silhouette identity; archive outgoing bank as Generation Nineteen
(creatures.json + MAX_ARCHIVE_GENERATIONS + both CSS families); rewrite clip notes; encode;
merge remote mains (no force); rebuild; rerun probe; deploy once (-Project upheaval -Branch main);
verify 200/media/archive/one noindex; kill background jobs.
