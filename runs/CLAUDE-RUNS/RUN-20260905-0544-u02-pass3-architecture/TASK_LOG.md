# Task Log: RUN-20260905-0544 - Creature 02 pass 3 architecture

**Created:** 2026-09-05 05:44 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-0544-u02-pass3-architecture/

## Objective

Architect (plan only, no implementation) the third pass on creature 02 against
OWNER-DIRECTION-3-2026-09-05.md (as amended with the owner's smear refinement:
"never clears is too much, but longer than usual in games. A bit glitchy.").

## What was read, in order

1. OWNER-DIRECTION-3-2026-09-05.md (binding; re-read after the in-place
   amendment folding in the smear refinement).
2. All three concept sheets (Side, Front, Description — the rear-oblique
   protruding-eye drawing informs the side-readability ruling R4).
3. RUN-20260905-0207-u02-pass2-implementation/REVIEW.md (no QA.md present)
   and TASK_LOG.md; RUN-20260905-0157-u02-pass2-architecture/PLAN.md; the
   three RECON-U02-*.md.
4. 09-ENGINE-GOTCHAS.md (§12: the s4 plates never selected the shipping rig —
   drives Stage L.5 and the judge-on-shipping-subject law), 08-LIGHTING.md,
   07-MOTION-STYLE.md, CLAUDE.md (both repos).
5. Source spot-checks to anchor the plan in real knobs: unnamed02_rig.h
   (kBNeck/kBLoopBase2 exist — the owner's "missing hinges" are a READ gap,
   not a bone gap), subject_u02_clip/subject_u02_s4 and the rig-selection
   plumbing in zhao_reel.cpp (kU02Sun* constants survive from pass 1;
   markers drawn by draw_zixx_moving_source_markers under moving_light),
   unnamed02_clips.h (hasty and drift are both circular — confirms the
   directional rebuild), unnamed02_fx.h (FxAnchors.ring exists; bullet
   ballistics are the wanderers).

## Deliverable

PLAN.md in this folder: 14 rulings, staging 0-L-M-F-A-G-X-Q, cost arithmetic
(three conduits ~58% of one pass ≈ 3.2% of frame clocks with the persistence
plane replacing stamp trails), deferrals and cut order.

## Headline decisions

- Lighting first (Stage L): sun per clip restored, ONE new subject
  unnamed02-inspect keeps the four lights, marker orbs removed u02-only,
  kU02SunRig (cool-cross-derived) as the u02 sun base rig; s4 diagnostics
  moved onto the shipping presentation.
- Mist = ambient ladder on kU02SunRig (thick AND translucent is a midpoint
  found by eye; the shell is fallback only).
- Face: shipping READ beats the trace (shorten/fatten almonds); partial
  outward yaw restored via yaw ladder for side readability; white repainted
  as a ring around the star; star grown; gaze clamped so stars stay inside.
- Antennae: tube ~0.7x, balls thickest (per-station inequality), visible
  junction knuckle balls at neck exit + re-entry; no new bones needed.
- Mana: opaque cores under additive halos ("filled"), ring-centre anchoring,
  2-3 continuous buzzing strands, decaying GLITCHY persistence plane with
  four named owner knobs and two shipped smear variants; six-variant menu;
  drip cut.
- Clips: hasty directional (straight crossing), drift rebuilt as lateral
  glide, fall 1.5-2x, idle slower, taunt hold-the-beat, headstand trick
  (authored ground contact, probed), lasso one attempt then cut.

## Status: COMPLETE — plan committed and pushed.
