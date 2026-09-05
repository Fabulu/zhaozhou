# Task Log: RUN-20260905-2036 - [Describe objective here]

**Created:** 2026-09-05 20:36 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-2036-manafold-p6-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 20:36 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-2036
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

## Manafold pass 6 — IMPLEMENTER

Lane: `C:/programmieren/zencrifice/manafold-p6-impl/{zhaozhou,Upheaval}` (own clones,
origin re-pointed at GitHub). The hardware lane and every other pass-6 lane are
untouched.

### 20:36 — orientation
Read, in this order: `PASS-6-ARCHITECTURE.md` (stages 0–G), `OWNER-DIRECTION-5`
(all subsections incl. 0-BIS/0-TER/0-QUATER/2a/2b/2c/5a/5b), `PASS-6-INPUTS.md`
(the 11-item protected list). Two coordinator corrections arrived mid-orientation
and are folded in:

1. **The eye pop-out IS sheet-backed** — `Description.png` inset, "abstehendes
   Auge schräg von hinten betrachtet", plus the artist's own sentence "die Augen
   stehen leicht nach vorne". It is a protected quality, not a tolerance. Still
   *leicht*: do not enlarge it either.
2. **`CONCEPT-DESCRIPTION.md`** (new, pulled): the star is **cyan/turquoise, not
   blue**; the **front sheet shows NO balls at all** (direct drawn support for
   deleting the separate ball parts); the eye is three nested shapes outward-in
   (deep purple lens → white star → cyan star), 4-pointed with **concave curved
   edges**, pair tilted into a **Λ**; **no nose, no mouth, permanently**; the
   **lightning is generated through the antenna** (the antenna is the emitter,
   which is why stage C matters twice over).

### 20:36 — stage 0.1, baseline binary
`build-direct.sh --output _build cel`, RC captured to a file (not `tail`'d — the
documented pipeline-exit-code trap). Building while I read source.

### Source map established (read, not assumed)
* `zhao_reel.cpp:4988` `u02_common()` — `s.cam_k = 240000` literal (stage A.2).
* `zhao_reel.cpp:5031` `subject_u02_clip()` — `s.creature = slot + 2`;
  slot→mana `slot==2 ? 9 : (slot==7 ? 0 : 3)`; smear `slot==2 ? 1 : (…7?0:3)`.
* `zhao_reel.cpp:7076` — `manafold-inspect` is the ONLY subject raising
  `creature_moving_light` (stage A.1).
* `manafold_model.h:185/200` `make_hinge`/`make_knuckle` — the ball parts to
  delete; `:100–180` the loop chain with `kLoopBladeRxMm[8]/kLoopBladeRzMm[8]`
  and the `taper()` lambda that will carry the swells (stage B.2).
* `manafold_model.h:254/290/322` `make_lens_teardrop`/`make_white_ring`/
  `make_star_blade` — the eye parts to replace (stage B.1). Note
  `make_lens()` at `:216` already exists as a *symmetric* ellipsoid A/B — but it
  is round-ended, and the sheet is **pointed at both ends**, so it is a starting
  point, not the answer.

### 20:52 — STAGE 0 done, STAGE A done. Both looked at.

**0.1 before-plates** captured from ONE self-built binary (`BUILD_RC=0` read from
a file, never from `tail`): `manafold-channel` (420 f), `manafold-hasty` (240 f),
`manafold-rest` (400 f), at HEAD, under `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross`. Baseline CRCs: channel `0xB489DCCD`, hasty
`0x1E3F51FB`, rest `0xBD1CBA93`.

**Looked at the baseline** (`evidence/00-before-channel.png`, 2x). What I see,
before any measurement: the antenna reads as a string of countable beads (§2b
confirmed by eye, not by a metric); the pink body carries a near-white blown
highlight over most of its lit face; the eyes are barely legible — a purple
sliver and a cyan dot; the creature is small in a frame dominated by the violet
bloom; and the pale gassy shell IS absent at f000 and present by f100, which is
the accumulation signature the architect attributed to the smear plane.

**0.2 diagnostics committed** (permanent, not lane-local):
* `manafold-still` — exposes clip slot 7, the 2-key form-diagnostic pose
  `build_still()` has always produced and nothing could render. 4 frames, no
  motion, no fold, no smear: the cheapest look at pure geometry.
* `manafold-fogprobe-mana` / `manafold-fogprobe-off` — the ablation pair.
* `slot < 15` → `slot < kKneadClipSlots`, where `kKneadClipSlots` is
  `sizeof(kKneadClipPm)/sizeof(...)`. The pass-5 fix changed `< 14` to `< 15`,
  which is the same bug one index later. The derivation now lives in the code.

**A.1 the many-colour rig on every clip** — `s.creature_moving_light = true` in
`subject_u02_clip()`. The reversal of Direction 3 §1 is written into the source
comment, and the `kU02Sun*` constants are kept DORMANT as the one-flag revert.
**A.2 the camera** — `kU02CamK = 360000`, named, in `manafold_art.h`.

**VERDICT BY EYE** (`evidence/A-rig-camera.png`, before/after at f000 and f210,
one binary each side):
1. **The camera is the win the inputs said it was.** The creature fills the
   frame; the eyes are legible *for the first time in this pass* — purple lens
   and cyan star both readable at native; the loop window is a pocket you can
   see strands inside rather than a distant sparkle. Nothing crops.
2. **The mana survives the rig** (risk 1, closed by looking): strands still read
   as strands in the window.
3. **⚠ THE PINK BROKE, and not in the way the architecture predicted.** D.1 was
   written for the OLD rig, where 49–57% of lit pink clipped at red 255 and the
   fix was to pull gain down. Under the moving rig at f210 the body is a dark
   red-and-purple plum: one flank hot red, the other deep violet, and the
   magenta-pink identity is gone. **The fault has changed sign** — it is no
   longer only "blown", it is "swung". Re-measuring before touching anything,
   per D.1's own first instruction.
