# Task Log: RUN-20260922-2146 - [Describe objective here]

**Created:** 2026-09-22 21:46 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260922-2146-manafold-pass25-final/

---

## Objective


Manafold pass 25, the FINAL pass. Three accepted mechanisms rolled out and tuned by eye: (1) bolt avoidance on all 22 live subjects with a raised clearance, because it currently passes close enough to read as crossing; (2) the ambient eye acting made a little stronger everywhere; (3) the rear-calm lever laddered on Hover, the real lever for the back ball. Full production: archive, exact bank, encode, publish, verify.

---

## Progress Timeline

### 2026-09-22 21:46 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260922-2146
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

### 2026-09-22 21:46 - Started
- Branches `manafold-pass25` created in both repos from the production-verified pass-24 mains (Zhaozhou `09108284`, Upheaval `8e45d38`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-26-2026-09-22.md`. The owner states this is the last pass.
- One Opus implementer, then a bounded independent review that publishes on PASS. No Qwen (reserved); GPT/Codex quota exhausted until 2026-09-25.
- Carried facts: only the fold figure edge links intersect the rods (free strands zero on all 22); depth-splitting is a proven no-op; `kRearCarrierCalmPm` was dead from pass 20 until pass 24 repaired it and measures ~52x the ambient scale.

### 2026-09-23 - where I am, written BEFORE reading the matrix
- Source complete for all three items; clean full rebuild RC 0.
- Item 1: rollout to all 22 through ONE shared accessor (`u02::bolt_avoid_for`),
  clearance 96 mm chosen by eye from the gate-clean rungs. Split retired from
  the bank, kept as declared dead code with a named probe in mbolt.
- Item 2: `kEyeAmbientOrdinaryPm = 800`, chosen by eye; contrast vs Curious/
  Startle checked at 800.
- Item 3: laddered the full range; the lever does NOT calm the back ball and
  the pass-24 "52x" reading was changed-pixels (an offset) quoted for a motion
  question. Ships the authored value, no byte change, with a positive-control
  leg so "changes nothing" cannot be confused with "is dead again".
- Gate matrix launched in the background (frozen copy, one invocation).
- NEXT AFTER THE MATRIX: production-ink every-frame sheets for Crackle, Hover,
  Inspect, Drift, Death Drop, Rest + one ordinary clip; then curate P25-LOOKS,
  write P25-IMPLEMENTATION.md, commit source then evidence, push.

### 2026-09-23 - source committed and pushed (451a9f11)
- Exact-off verified by hand before the matrix: `ZHAO_U02_BOLT_ROLLOUT=pass24` +
  clearance 46 + eye 600 reproduces the pass-24 bank on all 22.
- Non-live control subjects checked by hand (they are outside the identity
  legs): `manafold-still` and `manafold-nodule-solo` byte-identical;
  `manafold-crackle-legacy` moves under ITEM 2 ALONE, via slot 0's schedule
  entry. Recorded as open issue 4 rather than repaired -- exempting it needs a
  per-SUBJECT eye selector, a new mechanism on a final pass.
- Every-frame production-ink sheets rendered for Crackle, Hover, Inspect, Drift,
  Death Drop, Rest and Taunt II, frame CRCs confirmed equal to the shipping
  receipt. Looked at: continuous, no blackouts, no pops, loop seams close.
- Eye contrast re-made from the SHIPPING bank frames rather than from the ladder
  renders, so the plate is of what ships.

### 2026-09-23 - CLOSED
- Matrix 240/240 PASS, 0 FAIL, one invocation. The background task reported exit
  1 while the matrix reported MATRIX_RC=0 -- the 1 was my own trailing
  `grep '^FAIL'` correctly finding nothing. CLAUDE.md's exit-code law in a third
  costume; recorded in the report.
- mbolt header corrected (it still called B3 "CONTROL GROUP"); mbolt alone
  rebuilt and all seven legs/controls re-run green. Renderer NOT rebuilt, so
  every bank CRC stands against the binary that produced it.
- Commits: 451a9f11 (source), 306c6a63 (mbolt header), 82218e2a (evidence).
  Pushed to origin/manafold-pass25.
- Purge dry run: 0 candidates (everything inside the 48 h spare window).
  ~17 GB of .rgb intermediates under .tmp/ will fall due to the next pass's run.
- NOT done, by instruction: the 22-subject publication bank, the encode, the
  merge, the deploy. The coordinator sends the review/publish packet.

---

### 2026-09-23 - RE-OPENED: the BACK-BALL packet (one targeted packet, Opus)

The owner approved one more packet on item 3 after the rear-calm ladder proved
to be the wrong lever. Report: **`P25-BACKBALL.md`** (supersedes
`P25-IMPLEMENTATION.md` §3 on this item).

- **Diagnosed first, with a committed probe.** `tools/reel/manafold_backball.cpp`
  (`mback`, registered in `build-direct.sh`) decomposes the rear's posed-surface
  motion by muting one authority at a time ON THE POSED RESULT. Findings:
  the End swell's own position is the calmest back ball in the bank
  (0.99 mm/sample, 65 % of it the socket following the breathing body);
  90.3 % of its ORIENTATION is the rear rod aim; what an eye sees is carrier C
  and the C->End rod, 81 % "the antenna's upstream life" with **no single
  station over 9 %** because the chain is a travelling wave whose stations
  CANCEL -- freezing station B alone costs the last rod +146 %.
- **Hover was never the most violent rear in the bank.** Three clips shake C
  harder. It is the only live clip where the BACK turns MORE than the FRONT
  (1.08 against 0.39-0.99 everywhere else), on the long idle.
- **Repair: a filter, not a gain.** `kBackBallDampPm` -- a centred zero-phase
  moving average over the authored keys of stations A, B and C, run before the
  closure is solved. Shipped at 700 pm / 21 keys on bake slot 0, chosen by eye
  from a rendered ladder at native, 3x, 4x, 5x and 6x on both cameras.
- **Scope, declared:** slot 0 is hover AND inspect, one bake under two cameras.
  20 of 22 live subjects byte-identical FRAME BY FRAME (crackle included -- the
  same choreography on the separate bake, the containment proof).
  `ZHAO_U02_BACKBALL_DAMP_PM=0` reproduces the pass-25 bank bit for bit on 22/22.
- **Gate:** mback `--gate`, two ceilings and two ART FLOORS, all three controls
  fired (`--fail-undamped`, `--fail-window`, `--fail-dead`), the last reachable
  with legal stimulus.
- Matrix: `P25-BB-RECEIPTS/runmatrix_p25bb.sh`, one invocation, frozen copy.
- Commits: `b743640c` (source). Evidence to follow.
- **NOT done, by instruction:** the publication bank, the encode, the merge, the
  deploy. The coordinator sends the review/publish packet.

#### Where I was, written BEFORE reading the matrix
- Source complete and pushed; report, receipts and looks written; every
  by-eye judgement made and recorded.
- NEXT AFTER THE MATRIX: read it, record the tally in `P25-BACKBALL.md` §4,
  commit the evidence, push.

#### 2026-09-23 - BACK-BALL PACKET CLOSED
- **Matrix 265 / 265 PASS, 0 FAIL, one invocation**, frozen copy, `MATRIX_RC=0`
  (`P25-BB-RECEIPTS/gate-matrix-backball.txt`). Every pass-25 leg carried
  forward green with `DAMPOFF`, plus mback's four legs and 19 new selectors.
- Identity: `e-bb-off` reproduces the pass-25 bank 22/22 bit for bit;
  `e-ship` == the committed back-ball bank; `e-bb-scope` changes exactly
  hover and inspect.
- **Two matrix runs were stopped and discarded before this one**, and both for
  reasons worth keeping: the first measured the arrangement before
  `backball_damp` moved inside `finalize_rear_follow`; the second carried a
  scope leg that read a `.crc` no `bank` call writes, which would have printed a
  false red at the very end. The script was NOT edited mid-run -- bash reads by
  byte offset, and its own header records what that cost a pass-24 matrix.
- Three of my own instruments were broken in the same way the creature's four
  levers were, and all three are written down in `P25-BACKBALL.md` §5: a screen
  sweep saturated by the lightning, a `pgrep` liveness check firing on
  "command not found", and four monitors waiting for a line the script prints
  to stdout and never to the file.
- Commits: `b743640c` (source), `af8c97c1` (structural fix + report), plus the
  evidence commit. Pushed to origin/manafold-pass25.
- **NOT done, by instruction:** the 22-subject publication bank, the encode,
  the merge, the deploy. The coordinator sends the review/publish packet.

---

### 2026-09-23 - INDEPENDENT REVIEW: **PASS**

Report: **`P25-REVIEW.md`**. Evidence: `P25-REVIEW-LOOKS/` (10 plates, my own
renders), `P25-REVIEW-RECEIPTS/` (my gate logs, my CRC sets, findings as written).

- **Built the tree myself** into `.tmp/p25rev` (cel + 7 gates, ALL_RC=0, rc read
  directly). Binary MD5s DIFFER from the implementer's, so every check is
  behavioural rather than a hash comparison.
- **My renderer reproduces the shipping bank 22/22 byte for byte** against the
  committed `crcs-backball.txt`. Exact-off reproduces pass 25 22/22; all-off
  reproduces pass 24 22/22.
- **Non-monotonicity confirmed on all 12 rungs, independently**: 56(3) 66(8)
  70(11) 76(4) 106(1) dirty; 46/86/96/116/130/150/170 clean. Every rc, hit count
  and worst frame matches. `CLEARANCE_MM=70 --gate` -> rc 1, and the repaired B3
  goes red WITH B1 (pass 24's B3 would have printed a reassuring zero there).
- **The back-ball DIAGNOSIS reproduces number for number** on my binary
  (65.0 / 90.3 / 81.1 / -146.4). Verified, not taken on trust.
- **Visual: the back ball is calm and the antenna keeps its life.** Undamped the
  rear column's silhouette boils frame to frame; damped it holds one shape and
  drifts. Clearest on the fixed camera. The art floors (13.005 >= 11.5,
  1.850 >= 1.2) say the same from the other side.
- **Front-ball spin: NOT a problem, and it is structural.** Pass 24's lift is
  `tilt_front/yaw_front` -> `kBJunctionF`; the damp set is {HingeA,B,C} and
  JunctionF is upstream of all three. Measured: pass 24 delivered the owner's
  ask as TRAVEL (+2.4%) and in SPIN gave +0.3%, so a 35% spin cut cannot undo it.
  Travel preserved to +0.2%. Looked at: indistinguishable.
- **Crackle: RECOMMEND OFFERING IT.** Same choreography, same 1.08 ratio, and it
  is the worse case (long idle, fixed CLOSE camera). Not changed, per instruction.
  One table entry: `kBackBallDampClipPm[23] = 700`.
- **Every control fired by me**, each for its own reason: mback undamped/window
  breach the CEILINGS, --fail-dead breaches the ART FLOORS (legal stimulus, no
  mutant owed); all four mbolt controls plus both exit-code positives.
- **No bound relaxed** (the only removed constexpr in the whole diff is
  `kBoltRodClearanceMm = 46`, raised). **No gate default samples a subset.**
- Three RECORD corrections, all the same cause (numbers taken before the
  back-ball packet moved hover's rig), none changing a conclusion: two stale
  rows in `clearance-sweep.txt` (116, 130 - both moved the SAFE way, shipped rung
  unaffected); "54,595" is now 54,885; `P25-BB-LOOKS/README` calls plate 01
  consecutive when it is every-other-frame.

**NEXT: Part 2 -- archive pass 24, exact bank, encode, site, gates, deploy,
production-verify.**
