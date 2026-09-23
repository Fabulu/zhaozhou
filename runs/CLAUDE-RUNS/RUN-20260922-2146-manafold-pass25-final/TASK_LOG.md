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
