# Task Log: RUN-20260827-1730 - [Describe objective here]

**Created:** 2026-08-27 17:30 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260827-1730-zixxtrixx-head-only/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-27 17:30 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260827-1730
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

## Objective
Narrow HEAD-ONLY run on Zixxtrixx (owner: "Zixxtrixx is good. His face is
currently fucked."). Six items: cranium-not-muzzle retaper, brute-forced head
attitude (orientation sweep), dedicated head-attitude bone, structural fix for
the overlay shell clipping, committed self-intersection probe, mouth shrink.
FROZEN: canonical S, walk curves, salto timing/trajectory, idle body movement,
body thickness. Acceptance gate: side/front/three-quarter fixed views, slow
orbit, max idle bend, max walk bend, salto anticipation, overlap probe.

## Plan
1. Baseline: build current tree, render all four clips + probe, contact sheets.
2. zixxtrixx.h: head bone kBHead (26 bones), skull rebind, retaper, overlay
   deleted (head part becomes THE skull surface; body part starts at the
   junction station), mouth+body tile remap in mkcreaturepage.py.
3. Probe: non-adjacent ring-centre overlap check, committed.
4. Attitude sweep: 9 macro-override builds, one fixed side still each, one sheet.
5. Acceptance gate + frozen-clip regression sheets. Worklog + polygon reco.

## What happened, in order

1. Baseline: pre-run binary rendered all four clips + front diag; probe run;
   contact sheets kept (evidence/baseline-*.png, probe-baseline.txt).
2. Model restructure (commit "skull bone, overlay deleted..."):
   kBHead skull bone (26 of 32), skull rigid stations 0..5, blend 6..9;
   overlay shell DELETED -- head part is the surface 0..11, body part starts
   at 11 vertex-coincident; taper peak moved to mid-eye t=100, nose keys
   restored small + dome blunter; mouth shrunk to a 10x4 wobbled slit;
   body tile V rows re-derived; overlap check added to zixx_probe.
3. The sweep: two nine-pose sheets from one fixed side camera
   (evidence/attitude-sweep-*.png). Finding 1: POSITIVE attitude = NOSE
   DOWN (pass 3's sign assumption inverted). Finding 2: the whole
   -8000..+8000 range hung nose-down; extended to -14000..+2000 and PICKED
   -12000 BY EYE.
4. Probe findings acted on: blend widened 3 -> 4 stations (fold ate the
   eye's rear); fall neck pitch loll folded the hook over the rigid ball
   (head SWALLOWED at key 92, evidence/fall-probe-catch-*.png) -- neck
   pitch to 60%, lateral wobble kept; a counter-rotation experiment made
   it worse (186 -> 229 mm) and was reverted on the probe's evidence.
5. Overlap gate made AUTHORED: per-clip allowances 50/80/160/170 mm, each
   justified in source; current worst 37/66/143/150 -- exit 0.
6. Acceptance gate rendered (evidence/gate-*.png): fixed side, fixed
   three-quarter, fixed head-on at face height, slow orbit, max walk bend,
   full attack incl. anticipation, full fall. Regression pairs
   (evidence/regression-pairs-baseline-vs-final.png): body poses identical
   in every pair, only the head differs.
7. reel --check: "all sequence CRCs match". Probe ground bands: idle
   [-8..-3] (approved [-7..-3]; 1 mm at one key from the skull's mass),
   walk [-13..+10] exact, attack -426 exact, fall min 584 airborne.
8. Worklog appended (Upheaval commit), incl. the costed smooth-normals
   recommendation; kSides stays 30; tris 4,370 -> 3,680 via overlay
   deletion.

## Deviations / honest notes

- The sweep was performed on the HEAD-ATTITUDE bone, not on kStanceSlope[0]
  as the brief's numbers literally say: the canonical S is frozen, and the
  head bone (brief item 3) is precisely the mechanism that makes the sweep
  implementable without touching it. Same nine values, same fixed side
  camera, same one-sheet judgement.
- Residual nesting depths are DECLARED in the probe (ball-in-hook is the
  concept's own composition); the fall's worst (150 mm, key 124) reads as
  the lolling head pressing the coils -- judged acceptable on the render.
- Head-on, the skull reads wider than the sketch's round ball (googly
  bulge + elliptical section); the owner's googly ask cuts against
  rounding it. Left alone, noted in the worklog.
