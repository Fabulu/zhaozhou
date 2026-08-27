# Task Log: RUN-20260827-2140 - [Describe objective here]

**Created:** 2026-08-27 21:40 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260827-2140-zixxtrixx-round-skull-and-v2/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-27 21:40 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260827-2140
- Created working directory
- Initial context: [brief description]

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

## 2026-08-27 21:45 — plan settled after three coordinator updates
Scope grew mid-briefing: (1) dorsal pink must cover the whole upper surface
(body + head crown, ~40-50% of circumference, judged at the ~15 deg camera);
(2) eyes more front / pupil rotation / colour AFTER round skull and AFTER
direct colour; (3) Gouraud REVERSED BACK IN: qformats.md §8 (frozen) and
blocks.yml purpose lines already ratify per-vertex lighting + gradients —
zref implementing it is the oracle doing its job. Keep the per-row
barycentric re-evaluation model in rast.cpp (do NOT switch to a setup-side
plane form).
Order: goldens -> head ball + junction -> pink -> eyes -> RGB565+bilinear+mips
-> normals+Gouraud -> poly reduction -> amendments/proposal + worklog.
Baseline binary built from clean tree at f8f6681.

## Head: the cranium is a BALL (kBallNum)
- Goldens committed to Upheaval/creature/Zixxtrixx/golden/ FIRST (clip bytes,
  per-key pose CRCs, probe, 60 Hz sheets, source commit) — Wave 0 done.
- kEyeBulgeNum 85 -> 22; new kBallNum=280 envelope swells EVERY axis,
  smoothsteps from station 1, peak at 4 (5 dug the skull rear 75 mm into the
  dive stroke), falls to ZERO at the junction ring (kHeadEnd) — the skull
  grows out of the neck; junction ring stays bit-identical with the body part.
- head_ring() is shared by mesh builder and probe: the probe now measures the
  swollen vertical radius (honesty rule kept: in-plane axis).
- Attitude RE-SWEPT after the geometry change (evidence/sweep1-headzoom.png):
  -12000 still carries the skull level. KEPT, picked off the sheet.
- Overlap allowances re-authored on worst-key RENDERS (idle 80, attack 175,
  fall 200 — the ball nests deeper by design, like the sheet). Probe exit 0.
- Frozen check: pose CRCs + clip bytes IDENTICAL to goldens; probe bands
  idle [-8..-3], walk [-13..+10], attack -426 @56, fall min 584 — all exact.
- reel --check: all sequence CRCs match.

## Dorsal pink covers the whole top
- body_tile half 4.5 -> 13.0 texels (~41% of circumference), neck thinning
  deleted (it compensated the old 26-deg camera, which is gone).
- head crown pink_half ramps 3->13 from brow (y=20) to mid-skull, holds 13 to
  the junction -- one continuous pink top across head and body.
- Judged on renders at the current ~15 deg cameras (evidence/pink1-*.png):
  tq reads as the whole top; side reads as a broad dorsal region; front reads
  blue face in a pink cap, green chest -- Front.png's layout.

## Eyes on the ball
- EYE_ROW 19 -> 12: on the ball the rows forward of the radial peak face
  forward, so the eye's front edge wraps onto the frontal silhouette while
  staying mostly on the side (evidence/eye4-front-zoom.png: yellow reads
  left and right, like Front.png).
- EYE_ROT_DEG 12 -> -30: settled off an 8-angle fan of the PAINTED tile in
  true screen mapping (evidence/pupil-fan.png), confirmed on the reel zoom --
  the pupil is the sheet's vertical top-to-bottom band with middle swell.
- Eye COLOUR deliberately untouched: judged again after direct RGB565 lands
  (quantisation is flattening it; fix the cause first).

## GOURAUD in the oracle (N1+N2+N3) — the missing reference model, built
- SkinVertex gains a packed S1.7 bind-space normal (12 -> 15 B); (0,0,0) =
  no normal = flat fallback, so pre-normal assets render bit-identically.
- compile_creature generates area-weighted smooth normals, POSITION-KEYED so
  seam/meshlet-boundary duplicates share one normal (no lighting seams);
  micro rung recomputed from micro topology. Integer-only. The ring zipper
  winds inward under the double-sided raster: cross negated, verified by the
  first render (lit from above, not from inside).
- Lighting law (written as LAW): per-bone light pullback Lb = R^T L / bulk
  (one rhu division per component); per-vertex Lambert = skin-weight blend of
  the two bones' CLAMPED responses (N5 option (b): no renormalisation, no
  normal lane through the skin datapath); kSmoothMixNum = 819/1024 smooth vs
  face (owner knob); the existing per-channel rig composes gains per corner.
- rast.cpp: three Q16.16 colour lanes on ScreenV, TriMode.gouraud; row starts
  RE-EVALUATE the full barycentric form, pixels step affine — the per-row
  model kept DELIBERATELY (a setup-emitted plane would not be bit-exact with
  this oracle; the row walker owns the division in RTL too). Default off:
  every existing caller bit-identical.
- Textured path: interpolated gain replaces mod_*; untextured: pre-lit lanes.
- Rebuilt EVERY zref .cpp + archive (struct layout change). Probe exit 0,
  bands exact, pose CRCs + clip bytes IDENTICAL to goldens.
- reel --check: only creature-wave-walk and creature-bulk-pop moved (the
  point); re-pinned with provenance; all sequence CRCs match.
- THE FACETING IS GONE (evidence/gour1-side.png, gour1-tq.png).

## Direct RGB565 + bilinear + mips (T1/T2) — through the ONE TMU law
- zref_texture.hpp gains DirectPageSet (packed mip chains + tile bases + ONE
  mode word); rast.cpp's opaque path samples it through Tmu::sample ITSELF —
  the reel and the RTL share the law by construction, no re-implementation.
- Mode: RGB565, bilinear (frozen factored weights, half-texel bias), mips
  0..6, REPEAT around the ring (U), CLAMP along the body (V).
- creature_sim computes the per-triangle U4.4 req_lod from projected texel
  density (the Measure; the TMU contract excludes LOD derivation by design).
- mkcreaturepage.py emits kPageDirect: area-filter each level in RGB888,
  THEN quantise to 565 (never downsample quantised values). CLUT8 payload
  kept as the ordinary-creature tier + fallback.
- CreatureType.page_direct wins over page_set; only Zixxtrixx sets it, so
  the two --check creature subjects did NOT move again. Probe exit 0,
  --check all match.
- LOOK: the pixel read is GONE; eye colour visibly stronger with the shared
  palette out of the way (evidence/direct1-*.png). Crayon grain survives at
  zoom but is softened at distance — T5's multi-scale converter remains the
  counter, not done this run.

## Website: archive restructure (main session, parallel to the model work)

Owner call: *"archive the snakes we have now on the website, i have a feeling
they will be completely eclipsed by the new ones. Make an archive tab instead of
how it is now with archived ones."*

* **Snapshotted the four current clips first**, as
  `public/renders/archive-2026-08-27-{idle,walk,attack,fall}.{webm,png}`. This
  had to happen before the in-flight rework re-renders over the canonical
  filenames, or the record being asked for would have been destroyed by the
  thing it is a record of.
* `assemble.py` no longer gives each archived render its own tab. Archived
  renders collect behind a **single `Archive` tab**, newest first, each with the
  caption saying what was wrong with that pass. Only LIVE renders count toward
  `MAX_TABS`.
* Card now reads: Idle / Walk / Triple salto / Falling flail / Archive (6).
* **Gotcha recorded for whoever edits `style.css` next:** the panel-reveal rules
  are `nth-child`; the label rules directly above them are `nth-of-type`. That
  difference is load-bearing -- it is the only reason a `<div>` archive panel
  can sit among `<figure>` panels and still be revealed. Do not harmonise them.
* Committed and pushed as `d397ff0`. **Not deployed** -- publishing is an
  explicit call, and the sensible moment is when the new renders land, so the
  eclipse and the archive go up together.

## Poly reduction with better distribution (G2) — 3,680 -> 2,076 tris
- Variable radial detail: skull 22 (the ball + eye corners own the sides),
  neck 18, trunk 16, tail 10; blades 6 sides x 6 rings. The zipper stitches
  unequal counts; U is the ring angle so the texture never notices.
- Trunk is 16 NOT 14, and not for looks: a 16-gon at align 0 puts a vertex
  EXACTLY on the belly line, so the grounded run's authored sink survives
  the cut (at 14 the bottom chord rode ~3 mm high; idle band went to
  [-8..-1] -- one key from reading as hover). kTrunkEndStation=48 keeps
  every ground-touching ring on the 16 count.
- Probe: idle [-8..-2] (golden [-8..-3]; 1 mm at the shallow end, chord of
  the leaner ring), walk [-13..+10] EXACT, attack -426 @56 EXACT, fall 584
  EXACT. Overlaps all within allowances. Pose CRCs + clip bytes IDENTICAL.
- reel --check: all match (creature test subjects use their own types; the
  zixxtrixx counts do not touch them).
- LOOK: at 240p the 2,076-tri creature is indistinguishable from the
  3,680-tri one in every fixed view (evidence/poly2-*.png) -- exactly what
  smooth normals + filtered direct colour were supposed to buy.
