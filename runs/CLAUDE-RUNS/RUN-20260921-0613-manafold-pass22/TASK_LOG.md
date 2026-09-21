# Task Log: RUN-20260921-0613 - [Describe objective here]

**Created:** 2026-09-21 06:13 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260921-0613-manafold-pass22/

---

## Objective


Manafold pass 22 (small) from Owner Direction 23: the green and blue mana dots (fold motes) must SHRINK WITH DISTANCE, so distant clips such as Drift and Hasty stop showing close-up-sized dots. Lightning is out of scope. Reuse pass 19s distance law for the mote lines. Effects only, no rig or motion change.

---

## Progress Timeline

### 2026-09-21 06:13 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260921-0613
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

### 2026-09-21 06:13 - Started
- Branches `manafold-pass22` created in both repos from the production-verified pass-21 mains (Zhaozhou `b9de3059`, Upheaval `2afec1a7`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-23-2026-09-21.md`, including the owner clarification "They need to shink with distance".
- Small pass: one Opus implementer, then a bounded independent check, then publish. No Qwen (reserved); GPT/Codex quota exhausted until 2026-09-25.

## 2026-09-21 -- Manafold pass 22, sole Opus worker

Direction: `Upheaval/creature/Manafold/OWNER-DIRECTION-23-2026-09-21.md` plus
two same-day owner additions relayed by the coordinator (the lightning's knead
reaction; the ink as the named model for the dot law + "the lightning must not
change size from distance").

- READ FIRST, before any edit: Direction 23, P19-IMPLEMENTATION (the line law),
  P21-IMPLEMENTATION (the rig and gate state), CLAUDE.md.
- **Item 1 diagnosis, from the code, not guessed:** the fold-mote dots were
  NOT distance-scaled at all. `mana_fold` pushes halo 7..10 px + core 11..16 px
  and the renderer took `ms.r_px` verbatim for every non-LINE splat.
- Baseline built and CRC'd BEFORE touching anything (.tmp/p22/base): hover
  0x200AA3E7, inspect 0x1CFE8375, drift 0xFB17B7D6, hasty 0x425AA389.
- Measured the projected radii off the reel's own celmain telemetry: drift
  127.7 flat, hasty 117.1-138.4, hover 284.7-402.6, inspect 363.7-514.4. So
  **Inspect is the byte-exact near clip** (its minimum clears the ink's 360),
  and Hover is only partly exact -- worth knowing before claiming either.
- Item 1 shipped: `ManaSplat::dot` + `mote_push` + `mana_dot_r_px` on the INK's
  own ramp; three-way radius select in one renderer function; static_asserts in
  zhao_reel.cpp tie every mirrored knot to its kCelInk* constant.
- Ladder looked at (P22-LOOKS/01-04, notes in P22-NOTES/FINDINGS-02): the
  obvious law OVERSHOT and deleted the effect. Shipped ink @ strength 600.
- Item 5 shipped: roll + depth tumble + height shear on the placed figure.
  **First cut read as a wobble** -- cause measured with U02_FOLD_DEBUG: `dip_pm`
  peaks at 208 bank-wide, never 1000, so "49 deg at a full dip" was 7.5 deg in
  every frame that exists. Fixed with a declared, saturating reference.
  Notes in P22-NOTES/FINDINGS-03; ladder in P22-LOOKS/05-07.
- Gates: mrear R6 DOT (0x20) and R7 KNEAD (0x40). R6's census walks ALL 24
  clips (the pass-21 mrod repair and pass-19's 2-slot census, both remembered).
  R7 measures the real 18 stations on/off with a rotation- AND scale-invariant
  form descriptor, so a pure roll scores zero on it.
- Source committed and pushed at `379bd8bc` BEFORE the matrix run.
- **IN PROGRESS when the matrix was launched:** full gate matrix in one
  invocation into P22-RECEIPTS/gate-matrix.txt. Next after it lands: read the
  tally, then the combined-shipping looks, then P22-IMPLEMENTATION.md.
- NOT done, by instruction: no 22-subject bank render, no encode, no merge, no
  deploy.

### Matrix run 1 killed by editing the script it was reading

Worth writing down, because it is CLAUDE.md §11's live-tree trap in a costume
nobody warns about. The first matrix run reached 172 of ~180 legs; I read the
file, saw 7 FAILs that were all my own script errors, and rewrote the script
**while bash was still executing it**. Bash re-reads a script from the file as
it goes, so the byte offsets shifted under it and it resumed mid-token:

    gatematrix_p22.sh: line 157: syntax error near unexpected token `)'
    gatematrix_p22.sh: line 157: `e E) ------------------------------------------'

-- a fragment of a comment, which is what a corrupted resume looks like. It
died before the three live-history legs and the four identity legs, and the
`rm -rf` of the log directory I did at the same time produced the grep errors
that were the only visible symptom.

Two things I had also got wrong and that hid it:
* my wait condition listed the gate binaries but **not `zhao-reel-cel`**, which
  is what the selector and identity legs run -- so "no such process" was a
  statement about a subset, and the run was alive when I called it done;
* the `nohup ... &` replacement run never started at all (the script was broken
  at that moment), and its log file was simply absent -- exactly the silent
  `nohup` death CLAUDE.md records.

Fix, and the habit: **run long scripts from a frozen copy** in `.tmp`, so the
committed file can be edited freely, and let the harness own the process rather
than `nohup`. Re-run is from `.tmp/p22/matrix-frozen.sh`.

The 7 failures themselves were all mine, not defects:
* `n-mrod` and the four `d-mrod-*` legs were missing `--gate` (mrod requires a
  mode; without one it prints usage and returns 2).
* `r-rear-frame` wanted 0xB and `r-rear-joint` 0xA -- **stale expectations
  copied from the pass-20 script**. Pass 21's own receipts
  (`mrear-ctl-fail-rear-frame.txt`) already record 0x3 and 0x2: under the rods
  rig the legacy-root frame no longer trips R4 STRAIN. Checked against pass
  21's receipts before changing the expectation, not rubber-stamped.

### Final matrix: 179 / 179 PASS, 0 FAIL, one invocation

`P22-RECEIPTS/gate-matrix.txt`, from the frozen copy, `MATRIX_RC=0`.
14 normals, 10 mrear mask controls (each exact), 7 mrod, 37 mspan, 16 msmooth,
27 protected, 5 Wave-F, 56 selectors, 7 identity/live-history.

The three new pass-22 controls fired with their exact masks and nothing else:
`--fail-dot-scale` 0x20 (dots shrunk at far 691,182 -> 0), `--fail-dot-flag`
0x20 (dot population 691,182 -> 0), `--fail-knead-shape` 0x40 (rotation
43.00 deg -> 0.00, form 239.5 -> 0.0).

Identity: `e-identity-pass21` reproduced the pass-21 CRCs on all four witness
clips, and its positive control `e-dots-live` matched its own expectation while
differing from pass 21 on three of the four -- so the leg is live, not vacuous.

Then: 2.5 GB of `.rgb` render intermediates purged (CLAUDE.md). Curated
evidence committed is 1.8 MB, no raw frames.

## 2026-09-21 -- Independent review (Part 1): VERDICT PASS

Reviewer rebuilt everything and re-ran every claim rather than reading receipts.

- Built the pass-22 renderer AND a **fresh pass-21 renderer** from a worktree at
  `b9de3059`, so byte claims could be checked against a real baseline instead of
  a CRC against a printed CRC.
- **Exact-off: 1,740 / 1,740 frames byte-identical** across hover/inspect/drift/
  hasty. Identity legs are live -- the shipping render differs on all four.
- **Hover near read: PRESERVED.** Isolated the dot law (knead off): 180/600
  frames move, worst 3,270 px. At native the worst frame is indistinguishable;
  at 5x the motes are marginally tighter. Arithmetic agrees (16->14 at 285 px vs
  16->9 at Drift's 128 px). NOT a fault, NOT blocked.
- **Distant read: PASS.** Drift and Hasty both clearly improved; dots smaller and
  still dots. Rendered the strength ladder myself: **1000 really does delete the
  effect** (implementer's key judgement confirmed by eye), 800 borderline, **600
  right**. Would not move it.
- **Lightning size: VERIFIED** three ways -- structure (`dot` written once by
  `mote_push`, read once, `line` short-circuits first), the census re-run exactly
  (24 clips, 691,182/11,561,258/164,900, zero non-dot moved), and visually across
  four dot strengths where the strand never changes width.
- **Knead: PASS on both named acceptance clips.** Inspect and Hover both turn and
  re-form plainly at native; four press beats ramp cleanly; frames 0 and 599 are
  byte-identical so the loop seam is exact. Reference 150 is right for what was
  asked; the five shallow slots stay an open issue, not a blocker.
- **Gates:** fired all three controls from my own build -- each fires its exact
  mask and nothing else. Neither gate samples a subset (R6 walks all 24; R7 walks
  all and declares its 19). **Zero constexpr removed or changed.** Env bounds
  strict both ways (RC 2 on six bad values, RC 0 on two boundary-valid ones).
- Two minor instrument findings recorded in P22-REVIEW.md: **F1** R6's LINE
  far-leg is tautological (same expression both sides) -- does not weaken the
  claim, which rests on the plain branch and the near leg, but must not be quoted
  as if the far half were evidence. **F2** R7 gates on the bank MAXIMUM, so it
  cannot see the spread widening.

Proceeding to Part 2 (publish).
