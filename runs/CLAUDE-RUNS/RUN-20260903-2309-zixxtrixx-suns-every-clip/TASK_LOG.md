# Task Log: RUN-20260903-2309 - Direction 29: a sun in every clip, visibly lit, distinct colours; publish tonight

**Created:** 2026-09-03 23:09 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-2309-zixxtrixx-suns-every-clip/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 23:09 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-2309
- Created working directory
- Initial context: 21 of 22 bank clips have no point light; approved additive term does nothing for them. Read OD-27/28/29, 08-LIGHTING, additive-term report, RUN-2144 log.

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

## 2026-09-03 23:55 — sun system implemented; v1-v4 tuned by eye

Design: ZixxSunSpec per clip -- one point source tracking the terrain-snapped
instance at a fixed mm offset, ABOVE (+8.6 m) and to one camera-side flank
(azimuth authored per cam_yaw family), inner radius 15 m so the whole body AND
the salto dummy sit inside attenuation 1: a pure directional lambert sun; the
never-enters-inner-radius trap impossible by construction. Installed in compose
as a 1-source array with the additive gate raised subject-scoped (gate global
still default OFF); the Cool Cross rig underneath is untouched. ZIXX_SUNS=off
kill-switch = revert demo. Moving-light subjects keep their 4-source rigs.

Proof so far: pristine HEAD (c23c6a63) and my build with ZIXX_SUNS=off give
identical CRC on death (0x4B7B3E3E). NOTE: RUN-2144's evidence table CRCs
(e.g. death 0x221453EA) do NOT reproduce on pristine HEAD either -- consistent
with that run's own bookkeeping finding; my identity claim is against pristine
HEAD of this tree, which RUN-2144 separately showed matches the published webms
to encoder noise.

Tuning by eye at native + 2x pairs (base over sun):
* v1: mult dominants 1.25-1.55 -- stripe/crown flooded magenta on warm suns.
  Learned: the 1.0 gain ceiling means big mults just clamp; the ADD carries.
* v2: mults cut to ~0.62x -- barely moved (ceiling, as predicted).
* v3: adds raised ~1.5x (dominant 0.45-0.63). THE READ ARRIVES: walk's azure
  turns the head blue and lays a cool sheen down every lit face; death's deep
  red re-hues the head hot pink-red with a gold-olive lit back (red add over
  rig-lit green pigment = gold: physics, same as RUN-2144's lesson).
* v4: idle gold raised (620/330/30), taunt rebalanced rosier (560/25/380 --
  R+B adds on green were washing grey-pink).

## 2026-09-04 00:25 — v5: a sun is FAR AWAY; probe PASS; archive gen 18 staged

Caught before rendering: the probe's own apex figures (six-salto 12 m, the
nine-salto's authored 24 m root apex) sat ABOVE the first 8.6 m sun height --
the peak of those flights would have been lit from BELOW, the read the owner
has rejected twice. v5 moves every sun to 50 m up / 22 m lateral (elevation
~66 deg from the stage, the highest apex still ~50 deg below the sun) with a
65 m inner radius: every vertex of every flight and the target dummy stay
inside attenuation 1, so distance changes only direction -- a sun. Verified by
eye on salto-nine f200/f300 (pink lights the LEFT rim from above-left at
altitude) and salto-six f160 (gold on top of the wheel).

zixx-probe: PASS (committed pose/contact probe, all gates).

Site prep committed to Upheaval main (40e47dc): generation-eighteen archive
copies (22 webm + 22 png, byte-for-byte), creatures.json (gen entry, per-clip
sun notes, site note/tagline), style.css BOTH selector families 17 -> 18,
assemble.py MAX_ARCHIVE_GENERATIONS 18.

Plan for proof of "animation untouched", bounded to the question:
1. zixx-probe PASS (above).
2. ZIXX_SUNS=off full-22 render CRC == pristine-build (c23c6a63) full-22
   render CRC (the change is inert when off).
3. Ink-mask equality per frame between sunless and sun renders (the cel ink
   hugs the silhouette; identical ink sets = identical silhouettes = the
   motion itself unchanged, frame by frame).

## 2026-09-04 01:05 — bank rendered, looked at, and the revert path PROVEN

Full 21-subject v5 render complete; contact sheets (render/contact-sheet-*.png)
and direct pristine-vs-v5 pairs reviewed by eye: every clip visibly lit in its
own mood, no sun from below or behind (salto-nine checked at altitude), form
and pigment intact. The three subtle-looking thumbnails (balance, fall, attack)
were paired directly against pristine and all three read clearly at 2x native:
teal top and cyan head on balance, icy cyan coil on fall, blazing red eye ring
and gold-olive flank on the attack's five-second spear hold.

REVERT PATH PROOF (evidence-gateoff-identity.txt): suns build with
ZIXX_SUNS=off vs pristine pre-suns build (c23c6a63), all 22 subjects:
per-subject sequence CRC32C IDENTICAL, frame counts identical. Bonus
provenance: pristine moving-light = 0x65A8D1E5, exactly the CRC logged for
the PUBLISHED moving-light clip -- this tree is the published bank's source.

## 2026-09-04 01:30 — verification complete; encoding in progress

Motion-untouched evidence committed (evidence-motion-untouched.md): probe PASS,
22/22 ZIXX_SUNS=off CRC identity, ink-mask silhouette identity 6674/6674 frames
across all 21 subjects. The checker's border-touch column was its own false
positive (travel/flight clips legitimately reach the viewport edge; walk f0's
left-column diffs are hot-pink TAIL pigment re-shaded by the azure sun --
verified by pixel inspection, not assumed).

Bank accepted by eye: contact sheets of all 21 + direct pristine pairs for
balance/fall/attack (the three weakest thumbnails) -- all read clearly.
08-LIGHTING.md gains the Direction 29 section (four authored lessons).
tovideo.py encoding all 21 subjects.
