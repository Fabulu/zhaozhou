# Task Log: RUN-20260905-2121 - [Describe objective here]

**Created:** 2026-09-05 21:21 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-2121-manafold-experimental-mana-reel/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 21:21 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-2121
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

---

# THE EXPERIMENTAL MANA REEL (Direction 6)

## Objective
Ten videos. One shared subject + choreography, ONLY the mana configuration
differs. At least half must vary the MECHANISM, not the knobs. Judge at
native 384x240 on the shipping rig (`ZIXX_EXP=celmain`,
`ZIXX_LIGHT=diagonal-cool-cross`). Ship no constants.

## Lane
`C:\programmieren\zencrifice\manafold-mana-lab\{zhaozhou,Upheaval}` — own
clones, remotes reset to origin, reset to origin/main
(zhaozhou 18cce81c, Upheaval caaa6eb). The hardware lane and every
`manafold-p6-*` / `manafold-pass5-*` folder are untouched.

## Recon conclusions (2026-09-05 21:2x)
- Build: `bash tools/reel/build-direct.sh --output DIR cel`; READ THE EXIT
  CODE. Baseline build BUILD_RC=0 into `manafold-mana-lab/build`.
- Render: `zhao-reel-cel.exe OUTDIR subject...`; argv[1] is the output dir,
  argv[2..] the subject filter. `--help` is NOT a flag — it renders the
  whole bank (cost me one 2-minute background task; killed, verified gone).
- `channel` = mana candidate 9 (`mana_fold` aqua + `mana_lightning`), smear
  rung 1 (SHORT/CLEAN, tear 0), slot 2, 210 keys.
- The fold: 24 motes at fixed MVC weights over six posed antenna anchors,
  GRIP (anchor-polygon area) / KNEAD (anchor speed) / DRAG (hinge B lag).
- Root motion is just `c.root[f*3+0..2]` — a travelling clip is cheap to
  author. `drift`'s 6.9 m needed cam_k 185000; 3.6 m at 240000 should sit
  inside the window. VERIFY BY RENDERING, do not trust the arithmetic.
- `opaque` splats write `pal[t]` directly and `gain_pm` scales the palette,
  so a LOW gain on a normal ramp gives a near-black opaque blob. The
  negative-silhouette variant needs no new ramp and no fx.h change.

## Plan
New lane-only `tools/reel/manafold_lab.h` carrying a `LabVariant` table and
a FORKED `lab_fold()`, so `manafold_fx.h`'s shipped constants are never
touched. Additive hooks only: one clip (slot 15), one include, one
push_back, one call-site branch in `zhao_reel.cpp`.

## A theory checked and KILLED before it cost anything (2026-09-05 ~23:0x)
`three-stones` drew 450 mana px from four 19-26 px elements -- LESS than the
24-mote control's 495. The obvious explanation was the `fold_mvc` clamp: it
walks a stencil point 10% toward the pocket centre until it lies inside the
anchor hexagon, and gives up after 12 tries by returning the plain CENTROID.
If that were firing, the authored shape would be collapsing into a dot before
it was ever drawn -- which would have been the root cause of two passes of
"no nameable shape" and would have redirected this whole run.

Wrote `tools/stencilprobe.py`, a bit-faithful reimplementation of the clamp,
and measured it. **The theory is wrong.** At the shipping scale 300 the
collapse fires ZERO times on all six shapes and the drawn extent is 95-100%
of the authored extent. Even at 450 nothing collapses. The stencils reach the
pocket intact; the fold's failure to read is downstream of the geometry.

The real cause of the invisible stones was much duller: they were behind the
antenna arm, and their shine-through glow was at gain 300, below the point of
being seen. The probe is committed so the number is reproducible.
