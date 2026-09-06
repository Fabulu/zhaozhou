# Task Log: RUN-20260906-1748 - [Describe objective here]

**Created:** 2026-09-06 17:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-1748-manafold-p10-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 17:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-1748
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

## Progress Timeline

- **17:48** Run created. Lane `manafold-p10-qa/{zhaozhou,Upheaval}` cloned fresh
  from `origin/main`. Both clones carry pass 10: Upheaval `b25d59e`, zhaozhou
  `df9ae148`.
- **17:50** Read `PASS-10-FINDINGS.md` in full. Ten claims to attack, per the
  QA brief. Next: gate checklist, owner direction 7, architecture, then build.
- **17:52** Built cel, mprobe, c2proto, mband via `tools/reel/build-direct.sh`.
  All four RC=0 (read per-build, not the pipeline).
- **17:55** CLAIM 2 (mist-follow) CONFIRMED: one call site at `zhao_reel.cpp:3361`,
  no inline duplicate; equivalence 4000 steps 0 mismatches; gate traverse 9.00,
  follow=0 0.00, bob 1.00; selftest CAUGHT.
- **17:56** CLAIM 3 (joints_on_balls) CONFIRMED: five balls enumerated, injected
  pass-8 table drives the REAL function, 2 violations, selftest OK.
- **17:58** CLAIM 4 (C.2 NO-GO) CONFIRMED and the counterfactual DEMONSTRATED.
  Reintroduced the `continue` in a scratch copy: identity check reads 545 vs 991,
  drift 446, exits 2. With the identity check also disabled it prints
  **27.5 deg / VERDICT GO / rc 0** — the exact near-miss the pass reported.
  991 pm vs 1120 pm gate = 129 pm headroom: reproduced.
- **18:00** Next: the spine (claim 1). Need renders.
- **18:20** CLAIM 1 (the spine) reproduced EXACTLY from my own build:
  `150.5 48.7 98.5 / H330.7 / S172.5 / 0.0 deg`. And attacked properly: over the
  WHOLE coverage mask recovered from `U02_COVER_PAINT` (13,780 px), **0 differ**;
  rim 664 px, **0 differ**; interior 13,116 px, **0 differ**. No rim leak.
  Headline SURVIVES honest windowing (skin-only re-window: still 0.0 deg).
- **18:22** But TWO findings on the same table:
  (a) `bandwash.band_mask` admits **288 px of SKY** (y 0..8, 50-195 px from the
      creature) — 14.5% of the "antenna band" sample is not the antenna.
  (b) the published pass-9 leg reads **115.0 deg**; my shipped binary reads
      **149.1 deg**. The table says "all from ONE binary"; that third row cannot
      be, because A.2 raised `kMistAlphaMaxPm` 300 -> 380 after it was taken.
- **18:24** FALSE COMMENT SHIPPED BY PASS 10: `manafold_fx.h:520-527` still says
  "300 is the `mid` row" above `constexpr int kMistAlphaMaxPm = 380;`.
- **18:30** **CLAIM 6 REFUTED — THE C.1 LIVENESS GATE CANNOT FAIL.** Built the
  probe with `loop_pose` reverted to the pass-9 bind anchor (the exact fault C.1
  removes). The gate prints the IDENTICAL `78 mm ... LIVE`, rc 0. It rotates the
  bind anchor itself from `clip.quats[]` instead of driving `loop_pose` — it
  tests a COPY, the very fault pass 10 fixed in the bandprobe selftest.
- **18:35** Pass-9 baseline worktree at 8641fdc3 built (RC=0) for the Zixxtrixx CRC.
- **18:45** CLAIM 5 (R6 confound) CONFIRMED, BOTH readings, isolated harder than
  the pass did (one binary, one `#ifdef`, everything else held):
  * 0.1 extraction vs the pass-9 inline arithmetic: **0 of 400 frames differ**,
    identical `sequence_crc32c=0x9B32AB43`.
  * C.1 alone (posed anchor vs bind anchor): **131 of 400 frames differ**, worst
    frame 588 px. The published 128/401 reproduces. And this proves the C.1 FIX
    is real on screen — it is only its GATE that is fake.
- **18:50** CLAIM 8 CONFIRMED. The two clips are `rest` f200 (`A2-density-sheet`)
  and `hover` (`A2-hover-mid-vs-rich`). Stage A landed 2b7375b3, the `rich` row
  e8a750ce (15:37), both plates after. Judged after the exclusion, not before.
  BUT: the shipping promotion (4 constants, 300->380 etc.) is committed in
  **d42938bd, whose message is "run log: the encoder kill"** — the shipped
  density change is invisible in the log.
- **18:55** R4 on `manafold-hasty`, 240 frames, one binary: mean 0.90% / max
  1.95% of pixels move (findings say ~0.6-0.7%). 229 of 240 frames have a few
  px (max 20) that my RECOVERED mask does not cover — all at distance 1.0-1.4,
  delta ~8. Suspected recovery artifact (cover paint washed out by the smear,
  which composites after it), not a leak. Building a HARD mask dump to settle it
  rather than guess.
- **NOTE, my own trap, logged as it happened:** three `python` text-mode writes
  to the patched `zhao_reel.cpp` reported success and did not land (CRLF file).
  Caught only because I grepped the file instead of trusting the "wrote N chars".
  Fixed in binary mode with a read-back verification. Also caught myself reading
  `$?` after a pipe once — the exact CLAUDE.md trap.
- **19:10** CAUSATION PROVEN for the vintage finding. Rebuilt at the pre-A.2
  density (1700/260/135/300) and re-rendered: the pass-9 leg reads
  **91.3, 106.3, 128.5 / H215.7 / S73.8 / 115.0 deg** — the published row to the
  digit, every channel — and hasty f120 reads **0.64%** against the published
  0.6%. The shipped binary gives 149.1 deg and 0.95%. Not coincidence.
- **19:15** CLAIM 10 CONFIRMED. Pass-9 baseline (8641fdc3) built in this lane,
  16 Zixxtrixx subjects rendered from both binaries, `comm -3` empty:
  **16 of 16 sequence CRCs identical**, frame and colour counts too.
- **19:25** `PASS-10-QA.md` written with 34 evidence files. Verified no render or
  build process is still alive before reporting.

## Decisions Made

- Attacked the spine over the WHOLE coverage mask rather than the published band,
  because a band sample is structurally blind to a rim leak. Recovered the mask
  from `U02_COVER_PAINT` rather than re-deriving "the creature".
- Fed every pass-10 gate its own known-bad input rather than reading its code and
  reasoning about it. Two of three passed the fault.
- Proved the density-vintage finding by rebuilding at the old constants instead
  of arguing from commit timestamps alone.
- Did NOT repeat the coordinator's production-site check; extended it instead.

## Next Steps

- Report merged to `main` in Upheaval; push verified with `git branch -r --contains`.
