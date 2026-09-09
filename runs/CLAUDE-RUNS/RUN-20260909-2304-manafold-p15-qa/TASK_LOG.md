# Task Log: RUN-20260909-2304 - [Describe objective here]

**Created:** 2026-09-09 23:04 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2304-manafold-p15-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 23:04 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-2304
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

## 2026-09-09 23:05 — run opened, machine load checked FIRST

QA lane for Manafold pass 15. Lane: `manafold-p15-qa/{zhaozhou,Upheaval}` at
`67347e2a` / `8015536`, both clean.

**Load check before anything (the 2026-09-09 eight-hour lesson):** a build is
ALREADY running, and it is not mine —

    PID  6104 cmake --build build --target zhao-reel test_render_golden ... (started 22:59:40)
    PID 31208 ninja  zhao-reel test_render_golden ...
    PID  9844 cmake --regenerate-during-build -S C:\programmieren\zencrifice\zhaozhou -B ...\build

That is the MAIN tree, another lane's. **One build at a time** => my `--clean`
build is deferred until that PID set is gone. Non-build QA runs first:
document reads, the switched-on-state greps (checked item 2), gate source audit
for item 43 (what does each new gate report if the mechanism is ABSENT?).

Identify by command line, kill by PID, never `taskkill /IM` — nothing of mine to
kill yet; I am waiting on someone else's, not killing it.

## 23:20 — source-only findings, while the other lane's build holds the machine

Machine still at 100% CPU on the main tree's ninja/verilator build. No build of
mine started. Source-level work only.

**Item 2 (grep the switched-on state) — ALL CONFIRMED.**

    kFoldStrandOn          = true         manafold_fx.h:478
    kEyeGlanceOutPm[0]     = 867          manafold_art.h:1284  ({867,-400,640})
    kEyeTravelMaxDeg       = 45           manafold_art.h:1151
    wrap_root_delta = true  x4            manafold_clips.h (1698,2007,2053,3753)
    kU02BackdropBloom      = false        zhao_reel.cpp:5496
    kU02NightSunMagPx      = 25           zhao_reel.cpp:5539
    shell: 560/520/180/55/1600/{255,214,232}/260   all at the findings' values
    strand/shimmer: perSeg 6, core 2, coreGain 1000, dark 14, darkGain 1000,
      shimmerR 6, shimmerGain 620, flicker 380, hue 1, moteGarnish 720,
      kFoldMoteGarnishPm 300 (green fold untouched)
    kLoopArcMm[6] = {0,680,340,380,0,1280}   -- arc4 380->0, arc5 1160->1280 ✓
    kEyeDeformFollowPm=1000, kEyeSurfaceFollowPm=0, kEyeExpressLeanA16=1400

**REFUTATION 1 — `arcsweep.sh` CANNOT RUN ON THE TREE IT WAS COMMITTED TO.**
Its guard greps for the pass-14 literal
`{0, 680, 340, 380, 380, 1160}` and the shipped header is
`{0, 680, 340, 380, 0, 1280}`, so it exits 2 before the control row.

    $ bash Upheaval/creature/Manafold/probes/arcsweep.sh <lane>
    kLoopArcMm is not the value this sweep was written against; re-read it
    ARCSWEEP_RC=2

Its own header says it exists because "pass 9's sweep exists only as a table in
a comment; nobody can re-run it". It is now that. Checklist item 24 (a derived
constant invalidated when its input moves) landing on the probe itself.
Second half: the committed default sweep for the shipped branch is
`a5 in 1380 1460 1540 1620 1700`, but the findings' decision table is
`1240 1280 1320 1380`. **The shipped 1280 is not in the script's own row set**,
so running it as committed reproduces neither table.
(Trap restored the header; `git status` clean afterwards.)

**REFUTATION 2 — `manafold_eyecam.cpp` HAS NO BUILD TARGET.**
`grep -rn eyecam` over build-direct.sh, every CMakeLists and every .sh/.ps1/
.cmake in zhaozhou returns NOTHING. It is the sole source of LANE-EYE's
headline table (12-17% -> 86-100%) and its own last line reads "Committed
because a thrown-away probe is unreproducible (CLAUDE.md)". This is exactly
item 42, which LANE-FX fixed for `shellgate` (`mshell`) in the same pass.

Also: it does NOT call the production camera; `cam_for_slot` re-implements
`subject_u02_clip`'s table and its own header admits "It is a mirror and it can
go stale" -- item 10. I checked the mirror: subject_u02_clip (5641-5880) really
does only `s.orbit = orbit; if (!orbit) s.cam_yaw = 0x2000;`, so it is accurate
for the named clips -- EXCEPT that `manafold-crackle` and the six mana-menu
subjects are ALSO slot 0 and pass orbit=FALSE, and the probe hardcodes
`slot 0 => orbit`. Those shipped subjects get the wrong camera / no row.

**GAP 1 — the plan's structural census fix was not built.** PASS-15-PLAN §5:
"assemble.py gains a per-entry `generation:` line rendered on the card ... so a
stale entry can never again sit unlabelled." `git log -1 -- website/tools/
assemble.py` = `ed9bb4c`, 2026-09-06, pass 8. Untouched. The census (49f12e9)
is good work -- deleted eye tab, re-dated mana lab, 0 dangling refs, 0 missing
srcs -- but it is hand-typed labels, which is the thing that goes stale.

**Gate audit (item 43) so far, source-level:**
* `shellgate` check 6 IS mechanism-sensitive (a fixed-px band ties on r=10 and
  r=34 and fails). Its failable leg is a DEGENERATE INPUT (measure the big body
  twice), not an ablation -- weaker than item 43 asks, but the check itself is
  sound. Whole gate runs on a synthetic 96x96 disc: structure only, no evidence
  about the bank.
* `spangate` G6 is genuinely strong: distance (not vector), adjacent pairs only,
  through `decode_pose`/`deformation_frame`/`deform_skin_vertex_lanes`, and its
  known-negative ablates `strength_ex[0..3]` IN THE PRODUCTION STRUCT.
  ⚠ but a STALE COMMENT at lines 547-554 still describes the REFUTED v1 metric
  ("the VECTOR BETWEEN THEM ... the diagonal of that vector's bounding box"),
  contradicted 30 lines later. Item 13.
  ⚠ and it is a MILLIMETRE gate converted by an inherited `kMmPerNativePx = 7`.
  It renders nothing. The plan asked for "the number off the bank's own frames";
  this is off the bank's pose data.
* the "re-aimed nodule gate" is an 18-line PRINTF DISCLAIMER, not a re-aim.
* `eyesweep.py` floors: kSweepFloorPm 120, kTwoEyeFloorPct 85.0, authored from
  `hit` and `channel` only. It declares itself valid for 26 fixed-camera clips
  and has been run on 2.
* `mprobe`'s coincident-station exemption became a PROPERTY and got COUNT-GATED
  (expects exactly 2). No threshold loosened; the 1120 rim gate untouched.

## 23:40 — REFUTATION 3, and it is a NEW regression this pass shipped

**`eye_cam_az_a16` keys the camera mirror on the SLOT. `orbit` is a property of
the SUBJECT, and SEVEN SHIPPED SUBJECTS ARE SLOT 0 WITH `orbit = false`.**

    manafold_clips.h:642   if (slot == kEyeOrbitSlot) -> orbiting azimuth
    manafold_art.h:1396    kEyeOrbitSlot = 0   // "`hover` / `inspect`: orbit = true"

zhao_reel.cpp actually calls `subject_u02_clip(0, ...)` NINE times:

    8084  manafold-hover        orbit = TRUE    (correct)
    8107  manafold-inspect      orbit = TRUE    (correct)
    8467  manafold-crackle      orbit = FALSE   <-- LIVE ON THE PAGE
    8492  manafold-mana-aqua    orbit = FALSE   \
          manafold-mana-cyan    orbit = FALSE    |  the six mana-menu tiles,
          manafold-mana-blue    orbit = FALSE    |  all live on the page
          manafold-mana-green   orbit = FALSE    |
          manafold-mana-boil    orbit = FALSE    |
          manafold-mana-stack   orbit = FALSE   /

On those seven the new resting base
`45 deg * sin(cam_az - 90 deg)` is fed an ORBITING cam_az while the camera is
nailed at 0x2000, so the eye pair sweeps the full +-45 deg of base across the
loop against a static three-quarter camera -- wandering off the readable face
for long stretches. That is the exact picture D11 SS2.1 complains about,
NEWLY CREATED, in seven subjects that ship. Pass 14 had no base, so these were
unaffected before this pass.

`manafold_eyecam.cpp`'s `cam_for_slot` carries the SAME slot-keyed error, so the
probe cannot see it and its per-clip table has no row for any of the seven.

⚠ Three copies of this table now exist: `subject_u02_clip` (production truth),
`eye_cam_az_a16` (production RIG, load-bearing at render time), and
`cam_for_slot` (the probe). The two mirrors both say "a mirror can go stale";
the defence named in both is `eyesweep.py` on shipped frames -- and `eyesweep`
cannot certify on the merged palette (LANE-EYE's own SS6a) and has only ever
been run on 2 clips. **So the declared defence for this exact fault is the one
instrument this pass reports as inoperative.** The fault it was supposed to
catch is present.

**Arithmetic verified without a render:** cam_az 45 deg -> sin(45-90) = -0.7071
-> base = -31.82 deg, and the pin ladder's independently picked best tile is
-30. Agreement confirmed. cam_az 90 deg (camera square on the +X face axis)
-> base 0. The formula is right; its INPUT is wrong for seven subjects.

Still to prove on pixels once the machine frees: render `manafold-crackle` from
the shipping binary and look.

## Other source verdicts reached without a build

* **FX's "distinct CRCs COLLAPSED" does NOT weaken the identity verdict --
  CONFIRMED from source.** `bitident.py:125` scores
  `IDENTICAL if (crc_ok and sha_ok)` PER SUBJECT, and the sha leg is a sha256
  over every frame's bytes of that subject. A collision BETWEEN two subjects
  cannot make a subject match itself across binaries. FX's reasoning is right.
  ⚠ but `bitident.py`'s own docstring line 40-42 requires a POSITIVE CONTROL
  ("Build a deliberately mutated binary and this must go red, or the green
  meant nothing") and LANE-FX states it did not build one. Owed.
* **`kFoldFreeStrandOn` retirement LANDED and is correctly coupled:**
  `u02_free_strand_on()` returns `kFoldFreeStrandOn && !u02_strand_on()`, so
  with the strand on the free diameter is off, and `ZHAO_U02_STRAND=0` restores
  it -- which is exactly what makes FX's green-fold leg equal pass 14.
* **`manafold_rig.h`'s 46-line EYE diff is a pure extract-to-function refactor**
  (`eye_bind_x_mm`/`_z_mm`), identical arithmetic, no bone added or rebound.
  That supports the byte-identity claim structurally.
* **`kEyeDeformFollowPm <= 0` is an EXACT early-out** (`manafold_model.h`),
  leaving `deform_role` at kNone and emitting no sidecar. The known-negative is
  exact by construction, not by luck.
* **`U02_EYE_TRAVEL_PIN` bypasses BOTH base and glance** and the pinned branch
  never assigns `g.eye_lean`. Structurally consistent with byte-identity.
