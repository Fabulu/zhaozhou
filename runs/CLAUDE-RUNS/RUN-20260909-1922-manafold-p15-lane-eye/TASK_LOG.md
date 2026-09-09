# Task Log: RUN-20260909-1922 — Manafold pass 15, LANE-EYE

**Created:** 2026-09-09 19:22 UTC+02:00
**Status:** COMPLETE — lane can be deleted
**Lane:** `manafold-p15-eye/{zhaozhou,Upheaval}`
**Brief:** `OWNER-DIRECTION-11-2026-09-09.md` §2, via `PASS-15-PLAN.md` §1 / §2.

---

## Objective

D11 §2: the eyes do not move left and right; they should rotate with the angle
and rotate more for expression; they clip into the bouncing body. Three passes
had reported the first of these fixed.

---

## The finding, in one line

**The eye-travel channel was never dead. It was pointing the wrong way** — and
no measurement taken without the camera in it could have said so.

---

## Progress timeline

### 19:22 — read the brief, the plan, the art law, gotchas, the gate checklist
Accepted the architect's evidence rather than re-deriving it: the channel is
live at full amplitude and the axis is correct. The question is therefore what
the motion *does on screen*.

### 19:40 — built `manafold-eyecam`, the missing instrument
Nothing in the tree had ever put the eye's plate normal and the camera azimuth
in the same space. Two findings on the first run:

* both eyes readable in **12–17%** of frames on every fixed-camera clip, and in
  **NONE** on `hit` and `taunt3`;
* `kEyeYawOutA16`, commented *"partial outward yaw"*, measurably yaws the plates
  **inward** — L plate azimuth 99.05 while L *sits* at 61.74.

### 20:05 — the pin ladder, and it settled the pass in six tiles
`hit` f0 under the shipping env at pin −1000/−667/−333/0/+333/+867. The readable
band is **entirely negative**; the shipped schedule is entirely positive. 14 s of
render answered a question four passes had argued about.

### 20:30 — implemented: camera-relative base, camera-relative glance sign,
deform-follow authority on the eye vertices, the expression lean.

⚠ **The expression lean nearly landed as a no-op.** Every clip calls
`antenna_knead` first and `face_rest` second, and `face_rest` *assigns* the eye
quats — a roll composed in `antenna_knead` is overwritten two lines later,
silently, on every clip. That is the exact shape of the pass-12 fault this pass
exists to close. It rides the Rig instead, the way `g.nod` already does.

### 21:10 — proved both known-negatives EXACT
`kEyeDeformFollowPm = 0` + `U02_EYE_TRAVEL_PIN=0` reproduces the pass-14 bank
**byte for byte, 0 differing frames of 140**. Rebuilt with the constant at zero
to prove it rather than asserting it in a comment.

### 21:30 — built `eyesweep.py`, and it took four masks
Three were confidently wrong. A loose indigo rule selected **44,000 px of violet
night sky** and printed PASS while measuring it; a body rule normalised the
sweep by the width of the sunset; a brightness-free rescue let the sky back in.
Two were caught on frames where the answer was known, **the third by a picture
disagreeing with a number**.

### 22:10 — merged LANE-FX, rebuilt clean, and the gate REFUSED
Their shell washes the creature paler and the mask stops finding lenses, so the
gate failed at its calibration legs before reaching any verdict. **That is the
contract working.** I tried the difference-based rescue, found it could no
longer tell the fault from the fix on `channel`, and **stopped at the fourth
mask** — item 40's own conclusion — verifying the merged tree by a 3× crop of a
named frame instead.

### 22:40 — owner packet plate, findings, push.

---

## Results

| | pass 14 | pass 15 |
|---|---:|---:|
| both eyes readable, fixed-camera clips | 12–17% | **86–100%** |
| both eyes readable, `hit` / `taunt3` | **0%** | **89% / 82%** |
| lens centroid across the body (per-mille) | 630…854 | **186…664** |
| the same, at the shipped 39° peak | **877…1058** (past the limb) | — |
| lens area, median px | 190–265 | **302–350** |

---

## Commits

| repo | commit | what |
|---|---|---|
| zhaozhou | `0c97816c` | `manafold_eyecam.cpp`, the eye-vs-camera probe |
| zhaozhou | `5a44af35` | the fix: camera-relative travel, deform follow, expression lean, `eyesweep.py` |
| zhaozhou | `8bb4e24a` | eyesweep reads its own blobs; the palette contract declared |
| Upheaval | `aec3f48` | `PASS-15-FINDINGS-EYE.md` + `pass15-plates-eye/` |

All pushed to `origin/main`, rebased onto LANE-FX's work each time.

---

## Load rule

One build at a time, one renderer, **no encode**. Processes were identified by
command line and never by name; nothing outside this lane was touched. LANE-FX's
renders were running in `manafold-p15-fx` throughout and were left alone.

---

## Open, for whoever picks this up

1. **`kEyeSurfaceFollowPm` is a one-word owner question**, not unfinished work
   (`PACKET-sticker-or-cartoon-4x.png`).
2. **`kEyeYawOutA16` yaws inward.** Recorded beside the constant; the face is
   owner-accepted so the value was not touched.
3. **Wave 3 must re-authorise `eyesweep.py`'s mask against the final palette**,
   using its two legs, before believing any verdict it prints. The gate is
   twenty seconds for three legs of 140 frames, and the whole reason it exists
   is that this fault has been called closed three times on evidence from a tree
   that was not the one shipping.
