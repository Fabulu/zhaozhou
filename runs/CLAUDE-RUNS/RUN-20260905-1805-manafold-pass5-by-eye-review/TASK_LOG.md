# Task Log: RUN-20260905-1805 - [Describe objective here]

**Created:** 2026-09-05 18:05 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-1805-manafold-pass5-by-eye-review/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 18:05 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-1805
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

## By-eye review log

- 18:05 run init. Clones at zhaozhou f9bf26bf / Upheaval 78140d0 (both = expected heads).
- 18:07 built `zhao-reel-cel.exe` with `tools/reel/build-direct.sh` (no cmake; fit lane untouched).
- Instrument honesty (gate A): `rgbframe.py selftest` PASS. Subject builder read:
  `u02_common()` sets `s.sun = &kU02SunCalm` for EVERY u02 subject including the s4
  diagnostics, and each shipping clip overrides with its own named sun; only
  `manafold-inspect` raises the four-light rig. So the s4 form plates and the clip
  plates are both under the shipping presentation. Rendered with
  `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` set explicitly.
- Rendered: taunt2, rest, damage, hasty, hit, startle, curious, hover, u02-s4-front/side/tq.
- Contact sheets of EVERY frame for taunt2 (2x120).
- Fold telemetry (`U02_FOLD_DEBUG=1`) for curious: seg histogram 53 gather /
  48 hold / 51 knead / 28 release over 180 frames -> the 51 knead frames claim
  reproduces independently.
- 18:40 CALIBRATION AGAINST A KNOWN BUILD (gate item 4). First base build was
  5c40593f (the pass-5 open commit). It renders the creature **solid BLACK** —
  the untextured-black bug, because `manafold_page.h` was not committed then and
  the `__has_include` guard let it compile. That independently CONFIRMS pass 5's
  commit 209ae66a ("the repo can build its own creature again") fixed a real,
  reproducible, shipped-severity bug. Base moved to **209ae66a** so the creature
  is textured and the three pass-5 visual commits are the only difference.
- 18:45 SELF-CORRECTION (gate item 16 — "does your metric measure the same
  THING?"). My first badness metric counted near-white LOW-SATURATION pixels.
  Base-vs-head read 5-6x "more fog" at head. Looking at the plate showed the
  real difference is a HUE shift: the pre-pass-5 cloud was strongly CYAN
  (saturated, so my metric skipped it); the pass-5 cloud is WHITE-GREY (counted).
  The metric was measuring saturation, not coverage. Replaced with a
  hue-independent mana-coverage mask (B >= R and max channel > 170), which
  catches both the cyan and the white cloud and excludes the warm sky, warm
  ground and pink body.
- 18:59 STALE-BINARY TRAP, caught on myself (CLAUDE.md build note). The base
  rebuild at 209ae66a FAILED to link -- `ld: cannot open output file ...
  zhao-reel-cel.exe: Permission denied`, because a render still held the exe --
  and `tail -1` of the log showed "LD zhao-reel-cel", which reads like success.
  Every base plate and number produced before this point came from the 5c40593f
  (black-creature) binary. Discarded. Base rebuilt clean and verified against the
  commit's OWN headline figure: 209ae66a renders u02-s4-front with **803 unique
  colours** (the pageless build gives 77) -- the exact pair commit 209ae66a
  claims. Reproduced, not inherited.
- 19:20 Parked-item variants rendered from a THROWAWAY worktree (`zhaozhou-exp`,
  never committed) that exposes mote halo, mote count, stencil scale and the
  subject camera as env overrides. No shipped constant was touched. Seven
  variants at the same fold moment (curious f75, a HOLD frame).
- 19:35 Badness sweeps: fog/mana coverage per frame per clip; framing sweep
  (creature entirely absent / clipped at an edge); ink-outline sweep; smear-over-
  body depth check; loop-seam wrap-vs-adjacent deltas base and head.
- 19:45 Trajectories via the COMMITTED `trajplot.py` with `ZIXX_HIDE_CREATURE=1`
  background plates (its own selftest run first: PASS, the mask can fail).
- 19:55 FINDINGS.md written. Run closed. Background jobs verified stopped.
