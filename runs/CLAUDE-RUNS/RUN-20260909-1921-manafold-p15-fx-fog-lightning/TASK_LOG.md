# Task Log: RUN-20260909-1921 - [Describe objective here]

**Created:** 2026-09-09 19:21 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-1921-manafold-p15-fx-fog-lightning/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 19:21 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-1921
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

## 2026-09-09 19:21 — LANE-FX opened

**Lane:** `manafold-p15-fx/{zhaozhou,Upheaval}`. Upheaval `4c50975`, zhaozhou `12961f9e`.
**Brief:** OWNER-DIRECTION-11 §2.3 (the fog) and §4 (the lightning). PASS-15-PLAN §2 B.3, §4, §6.
**My files, exclusively:** `manafold_fx.h`, `zhao_reel.cpp` (shell call region + env knobs), `rungsweep.py`.
**Not mine:** `manafold_clips.h`, `manafold_art.h` eye block, `manafold_rig.h`, `manafold_model.h` (LANE-EYE).

### What I read and what it settles

* **The fog has never been fog because it is an ANNULUS THAT ISN'T ONE.** `shell_paint`
  is a screen-space distance-from-silhouette band: 3 px out, 6 px in, peak alpha 440,
  effective ~27% one ring out. At the eye/body intersection (tens of px interior) it is
  **identically zero**. No alpha fixes that. The architect proved it on pixels
  (`B-shell-ladder-3x.png`): 440 = edge anti-aliasing, 900 = a white rim glow.
* **The lightning surround is black by arithmetic**, not by tuning:
  `kManaStormMid={8,12,46}` x `kFoldStrandDarkGainPm=300` = ~(2,4,14). The pass-14
  ladder swept radius and gain and never swept HUE.
* `kFoldStrandOn` (`manafold_fx.h:364`) is **false**. The connected figure exists behind it.

### Plan

1. **FOG:** re-author `shell_paint`'s geometry as a real annulus scaled to the body's
   own apparent radius (uncapped inward distance transform, R = max depth, annulus
   depth a per-mille fraction of R). Density rises inward to a peak at the annulus's
   inner edge, then holds at a core floor -- which is what the column integral through
   a real fog shell actually does, and it is also the profile that makes an eye sinking
   into the body FADE. Ladder `U02_SHELL_REACH` on both backdrops with a too-far rung.
2. **LIGHTNING:** three layers -- white core (keep), **NEW additive blue shimmer**
   (the missing thing), thin per-backdrop navy backing with its RAMP raised off black.
   More motes. Flip `kFoldStrandOn`. Keep the green/false fold separate and intact.
3. Everything through `rungsweep.py`, ONE binary, both backdrops, provenance beside
   each plate.

### Discipline
`--clean` builds always; real exit code, never a pipeline's; `rgbframe.py` only;
contact sheet finds, 2x crop confirms; calibrate on a known-negative; PIDs never
`taskkill /IM`; one build, one renderer, NO encodes.

19:21 baseline `--clean cel` build started (`build-base.log`), output `build-fx/`.

## 19:45 — the code is written; waiting on the clean build

**FOG — `shell_paint` re-authored (manafold_fx.h).** The band is gone; the shell
is now an annulus scaled to the creature's own apparent radius:

1. an **uncapped inward distance transform** of the renderer's cover mask;
2. **R = the deepest interior pixel** = the body's apparent radius THIS FRAME
   (so the fog breathes with the bounce);
3. **one continuous profile from the gas's outer edge inward** -- rise to a peak
   at the annulus's inner boundary, decay to a plateau, hold.

The profile is not a guess. The column integral through a real fog shell is zero
at the silhouette, peaks where the ray goes tangent to the clear core (screen
depth R - R_core, i.e. the annulus's inner edge), then holds at about half that
across the interior. That is ALSO the owner's sentence verbatim, and it is ALSO
what makes a sinking eye fade instead of cut. Three readings, one curve.

Knobs, all env-overridable from one binary: `kShellFogDepthPm` (380),
`kShellOutReachPm` (55), `kShellCoreFloorPm` (340), `kShellRiseGamma` (1600),
`kShellAlphaMaxPm` (440, re-laddered from scratch -- the pass-12 pick was
"the most you can push a fringe" and does not transfer to a volume).

**LIGHTNING (manafold_fx.h + zhao_reel.cpp env).**
* `kFoldStrandOn` **flipped to true** -- D11 calls this the STANDARD, not an
  experiment. Declared loudly; `channel` is protected content and this changes it.
* `kManaStormMid/Hi` **{8,12,46}/{18,26,78} -> {12,20,64}/{26,44,112}** and
  `kFoldStrandDarkGainPm` **300 -> 1000**. The navy is now IN the ramp where the
  opaque+soft blend actually reads it, and the "LOWER IS DARKER" knob nobody
  could read the direction of is gone.
* **NEW `kRampShimmer` + a third draw pass** -- additive blue, depth off, between
  the navy backing (9 px, was 12) and the white core (3 px). Per-stamp per-FRAME
  flicker at +/-380 pm is what makes it shimmer rather than sit.
* **NEW `kRampBoltCore`** and the SHAPE motes join the lightning vocabulary when
  the strand is on; wanderers do not (they are not folded into anything).
* Mote garnish 300 -> **720 pm** while the bolt folds ("we want more of them").
* The green/aqua fold keeps every number it ships with. One rebalance rung
  (`ZHAO_U02_AQUA_BAL=1`, B >= G) as an experiment row, default off.

**GATE.** `manafold_shellgate.cpp` re-aimed: checks 2, 3 and 4 would have passed
TAUTOLOGICALLY on the new mechanism (2 read a pixel the annulus also paints; 3
asserted the opposite of the owner's sentence; 4 compared the ink against a
neighbour that is denser anyway under a rising profile, so kShellOverInkPm=1000
would have passed). New check 6 is the mechanism itself -- the fog must reach
deeper into a bigger body, which a fixed-pixel band cannot do.
**And it had NO build-direct.sh target**, so nobody had run it since it was
written: 10-GATE item 42. Added `mshell`.

**Trap paid for, in the log because it is generic:** a `\n` written into a C
string through a Python heredoc arrived as a REAL NEWLINE and split two string
literals. 09-ENGINE-GOTCHAS s21's family. `BUILD_RC=1` while the harness
reported the task "exit code 0" -- the outer subshell's status. Reading the real
exit code caught it in one look.

## 20:05 — rendered, and LOOKED. Two verdicts, one of them a fault I made.

Binary `690a8c91` (baseline was `dae2fa57` -- the md5 moved, so the header edits
are in). Rendered `hover`/`hit`/`channel` under `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross`. Plates in `diag/plates/`.

**FOG: it works, first time in five directions.** `fog-hover-BA.png` and
`channel-BA-f250.png`, native 3x crops, P14 beside P15 from two binaries
(labelled as a BEFORE/AFTER pair, not as a rung -- a ladder comes from one
binary and this is not one). The P14 ball is a hard-edged solid with a black
line round it. The P15 ball wears a soft gas layer thicker than any outline,
with a visible inward gradient, and the ink is still solid black. On the violet
night the mood survives. **The shell is finally fog rather than anti-aliasing.**
Open question for the ladder: the interior plateau lifts the body's pigment a
little. That is the D8 §4 knob (`kShellCoreFloorPm`) and the owner picks it.

**LIGHTNING: the shapes are BEING MADE -- and they are a white rope.**
`p15a-hover-sheet.png`, every 12th frame of `hover`: a closed figure is traced
in the antenna window on essentially every frame, and it CHANGES -- a ring at
f036, a wide loop at f120, a triangle at f384, an arrowhead at f420. That is
D11's "they need to make the shapes", and it is the first time this creature
has done it. But the line is a uniform saturated white tube. No blue, no
filament, no bolt.

⚠ **And I can name the cause without a ladder, which means I must PROVE it with
one rather than act on it.** `kFoldStrandPerSeg = 6` subdivides each
sub-segment into 6 stamps -- and pass 14's own note records that a sub-segment
is about THREE MILLIMETRES, which is one to two pixels. So the six stamps land
on the same pixel. **perSeg is not a connectedness knob at this scale; it is a
6x additive overlap knob**, and 09-ENGINE-GOTCHAS' own strand lesson is that
the white is set by OVERLAP and not by gain. The line was already continuous at
1. Next: a perSeg ladder on `channel` from one binary, with a mote-count rung,
so the claim is proved on pixels instead of asserted from a comment.

Also visible on `channel` f250: the navy backing IS reading, as a dark rim
around the white mass. The layer works; it is being swamped.

## 20:25 — the shell's render cost, removed; and where the lightning stands

**PERF, and it is a real regression I created.** The first `shell_paint` cost
about **1.8x the reel's whole render throughput** (~200 frames/min -> ~112,
measured on the wall clock across two renders of the same subjects). Across a
28-clip bank that is half an hour added to every publish wave. Cause: it
allocated and zeroed FOUR frame-sized vectors per frame and materialised every
EXTERIOR pixel as a BFS seed -- ~85,000 of them, each fanning out to eight
neighbours -- when level 1 is just "a cover pixel touching a non-cover pixel or
the frame edge", which is one scan. `depth` now also carries the outward skirt
as NEGATIVE distances, so two of the four buffers stop existing, and it is
reused across frames.

**The optimised version reproduces the gate's profile DIGIT FOR DIGIT**
(`13 33 57 88 121 161 205 254 227 227 197 170 141 141 114 85 85 ...`), which is
the corroboration s16 wants before believing a rewrite.

**LIGHTNING at 5x on the night (`interim2.png`, channel f363):**
* STRAND OFF (the shipped look): a cloud of aqua blobs. No shape. This is the
  owner's complaint, reproduced from my own binary as the control.
* STRAND ON: **a spiral drawn in white filaments with a blue shimmer hugging
  them and a navy backing outside that.** That is D11's sentence, on screen.

**And at 2x/native (`native-check-2x.png`), the honest test (item 9):**
* NIGHT reads. The spiral and its blue survive.
* **DAY does not.** On the pale sunset the blue washes out and the figure is a
  white loop. That is 08-LIGHTING's backdrop law arriving exactly where it said
  it would -- an additive effect cannot win against a bright field, and the two
  clips whose job is the mana are the ones that carry it.

So the next axis is the NAVY: deepen and widen the backing so the shimmer has
something dark to be bright against on the day too. The pass-14 review's guess
was that a better surround may collapse the per-backdrop split; that is now a
testable rung rather than a hope.

**`manafold-hit` is the right ladder subject** and I had been about to use
`hover`: 140 frames instead of 600, it carries the fold figure (spirals at
f010-f025, a triangle at f110), it is on the DAY backdrop where the fault is,
and it is also the eye-clipping clip. Three questions, one 35-second render.

**Fog, by eye on the hit sheet:** the body reads noticeably WASHED across the
clip -- a dusty mauve where P14 is hot pink. `kShellCoreFloorPm` (340) is the
knob and my own eye says it is too high. Into the ladder, with the owner's pick
above mine.
