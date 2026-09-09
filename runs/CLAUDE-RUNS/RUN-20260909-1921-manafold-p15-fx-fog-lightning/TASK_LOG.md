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
