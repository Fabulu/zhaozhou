# Task Log: RUN-20260910-0610 - [Describe objective here]

**Created:** 2026-09-10 06:10 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0610-manafold-p15-lane-eye-2/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-10 06:10 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260910-0610
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

## 06:15 — orientation done, plan fixed

Read: PASS-15-QA §1 (R-1/R-3/R-6), PASS-15-REVIEW §2/§5/§6, PASS-15-FINDINGS-EYE
(all), D11 §2, 10-GATE-CHECKLIST §0 + 40..43.

**The structural fact that decides the fix shape, and neither QA nor the review
had it:** `manafold.h` compiles ONE CLIP PER SLOT and the reel plays it by
`slot_id`. The eye base is BAKED INTO THE CLIP. So `subject_u02_clip(0, ...)`
with `orbit=false` cannot be given a different eye base at render time -- it is
playing the same bytes `hover` plays. **A per-subject flag on the rig call is
not reachable from the reel; the camera has to be baked WITH the pose.**

So: the idle exists TWICE. Slot 0 = the orbiting bake (hover, inspect --
byte-identical, untouched). Slot 23 = the same choreography, same SCHEDULES
(slot 0's glance phase / knead gain / nodule table), baked for the fixed
three-quarter camera. The seven wrong subjects move to slot 23.

Mirror count 3 -> 1 definition + 1 ENFORCED join:
  * `u02::clip_cam_orbits(slot)` is the single definition. Bank builder, pose
    layer and probe all read it.
  * `s.cam_yaw = 0x2000` and `kEyeCamYawDeg = 45` collapse into ONE constant,
    `u02::kU02FixedCamYawA16`, written by subject_u02_clip and read by
    eye_face_base_a16.
  * `subject_u02_clip` now ABORTS if the subject's own `orbit` argument
    disagrees with the slot's baked camera. That is the join that broke, and
    the two operands are clocked separately (a hand-written call argument vs a
    bank property), so the detector is not blind to the fault it names.
  * `cam_for_slot` in manafold_eyecam.cpp is DELETED; the probe reads
    `clip_cam_orbits`.

R-3: add the `meyecam` target to build-direct.sh (LANE-FX's `mshell` pattern).

### Where I am / next step
Next: edit manafold_art.h eye block. Nothing rendered yet. No build running.

## Close-out

**Delivered:** `Upheaval/creature/Manafold/PASS-15-FINDINGS-EYE2.md` +
`pass15-plates-eye2/` (10 plates).

**zhaozhou commits** (each built, run and pushed before the next):

| | |
|---|---|
| `c269b448` | R-1 + R-3: the idle baked twice, one definition + one checked join, `meyecam` target |
| `6c078317` | R-6: eyesweep refuses instead of grading when its lens rule is blind |
| `9877612d` | the camera-join guard's committed positive control |
| `6c2fc2fb` | the surface-follow ladder -- and it refutes the remedy it was built to offer |

**Upheaval:** `33b4c05` (rebased onto LANE-FX-3's three landings).

### The three things worth carrying forward

1. **The pose is BAKED PER SLOT.** Neither QA nor the review had this, and it is
   why both proposed fix shapes were unreachable. Any future "make this subject
   pose differently" on this creature is a bank question, not a reel question.
2. **The same instrument told me opposite things on two trees, honestly both
   times.** eyesweep said my fix did nothing (bleached palette, mask blind on
   364/600 frames, `lens_px = 0` on frames showing four whole eyes); after
   LANE-FX-3 un-bleached the creature the same gate showed the lens area at 2.6x.
   The picture was right at both moments. The gate now refuses rather than grade
   when it cannot read.
3. **The surface-follow ladder refutes the review's own remedy.** At pm 1000 the
   white splinter is still there. Re-filed against the eye's proportions.

### Machine hygiene
One build at a time, one renderer, never the bank; no `quartus` alive at any
point; nothing killed, so no `taskkill` question arose. Both repos taken forward
by **rebase**, never a blind merge -- three separate FX landings arrived mid-run
and the last one moved my numbers, so §2 of the findings was re-measured on the
merged tree (QA's R-4 in reverse).

**Intermediates purged:** ~5 GB of my own `.rgb` plus a 114 MB `diag/` left by
the previous eye lane. **0 `.rgb` files remain in the lane.** Scratch worktree
removed and pruned.

### Where I stopped
Nothing in flight. No background task running. Lane can be deleted for the
camera join; the eye's open work (splinter, dagger proportions, white rim, the
owner's sticker-or-cartoon call) is shape work and needs the owner, not an agent.
