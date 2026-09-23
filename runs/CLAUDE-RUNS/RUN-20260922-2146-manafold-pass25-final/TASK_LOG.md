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
