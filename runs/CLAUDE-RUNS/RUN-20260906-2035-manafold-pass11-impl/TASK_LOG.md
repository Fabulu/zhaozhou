# Task Log: RUN-20260906-2035 - [Describe objective here]

**Created:** 2026-09-06 20:35 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-2035-manafold-pass11-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 20:35 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-2035
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

## 2026-09-06 — pass 11 implementer

Lane: `manafold-p11-impl/{zhaozhou,Upheaval}` cloned from `origin/main`
(zhaozhou 3b2b8b91, Upheaval 356fd65). Plan: `PASS-11-ARCHITECTURE.md`,
order 0 -> S -> M -> F -> F.5 -> E, P interleaved, L spawned.

### Log

* Built `cel` via `build-direct.sh --output ../build` — BUILD_RC read from the
  build, exe present.
* **STAGE S rendered FIRST** (Direction 8 §2's discriminator):
  `manafold-fogprobe-mana` (mana ON, smear OFF, mist OFF) with
  `U02_FOLD_DEBUG=1`. RENDER_RC=0, 400 frames, crc 0x60245B52.
  Fold segments on slot 5: drift 0-51, gather 52-123, hold 124-238,
  **knead 239-371**, release 372-399. Sampled INSIDE the knead window
  (250/270/300/330) — never from a drift frame.
  **VERDICT: the shapes are DRAWN.** Cyan particle figures ride the loop in
  every knead frame; f330 shows a closed ring. So it is Direction 8 §2
  **Outcome A — occlusion**, not a regression. Stage M's cut is the fix.
  (Read quality note for M: the figures read as a *swarm* rather than a
  crisp drawn edge; judge again once the mist is off them.)
* ⚠ **BLOCKER FOUND: disk C: at 100%** (139 MB free of 952 GB). A 400-frame
  render is ~110 MB, so `manafold-rest` died mid-write twice with
  `cannot open .../0038.rgb` — which looks exactly like a render bug and is
  not one. Reclaiming the session scratchpad (41 GB of superseded render
  output from passes 6-10). **Recorded because a full disk imitates a
  rendering fault**, which is the stale-binary trap wearing another costume.

### WHERE I WAS (per rule 0.4) — before attending the delete

In Stage 0: repairing 0.1 (mist-follow gate -> `g_u02_mist.follow_pm` plus a
`kMistVariants[]` walk) and 0.2 (C.1 liveness gate rebuilt to call
`loop_pose`). Next step after: prove both failable through the SHIPPED path.

### Stage M closed (pushed 52e50375)

The two things I tried first were WRONG and the plates say so:
alpha 380->60 left the footprint identical; feed_of_halo 1300->450 barely moved
it. The plane saturates against cell_cap_pm, so at steady state the per-frame
feed decides almost nothing. **The fault was EXTENT, and extent is
`kMistKeepPm`.** 930 -> 420. Shipping row `smidgen`: 180/220/1300/110/keep 420.
Shell `kFogThicknessPm` 4500 -> 1200, confirmed on `channel` as well as `rest`.
Exclusion 0 of 5,674,554; failable leg 15.3%.

### Direction 8 §6 landed mid-pass (coordinator relay)

* **E.2 `bar-cyan-fat` is RELEASED from the fence — ship it in pass 11.**
* F.0 framing is now a PREREQUISITE: the owner judges MY fold after this ships.
* Eye travel 14° authored / 22° clamp — recorded, NOT shipped.
* Blink — do not start.
* Report whether this lane can be deleted; `tools/lanes/lane-audit.sh`.

### Stage L reported back

The white smear inside every folded shape is the **edge halo's RADIUS**, not the
motes and not a gain. `kFoldEdgeHaloRPx` 8->5, `kFoldEdgeCoreRPx` 3->2.
`kFoldEdgeCoreGainPm` confirmed dead AND confirmed inconsequential.

### WHERE I AM (rule 0.4)

Next: E.2 (released eye fix), then **STAGE F, F.0 framing first**. F is the
spine and is never trimmed. Remaining after F: P.1/P.2/P.4, publish.

### Stages 0, S, M, E.2, F.0-F.4, P.1-P.3 all landed on `main`

### WHERE I AM (rule 0.4) — the full bank render is running

Started `bank.sh`: all 22 live subjects into `Upheaval/website/scratch-reel`.
**No rebuild and no source edit until it finishes** (the live-tree trap).
Status accumulates in `bank.status`, one line per subject with its own RC and
frame count — read THAT, never the pipeline's.

Next steps, in order:
1. (while it runs) write `PASS-11-FINDINGS.md`. No build, no source edit.
2. When the bank lands: re-render `manafold-antenna-fixed` + `-quarter` for the
   **F.5 plate package** — the F3/F4 plate renders predate the lead-boost
   backoff 1450 -> 1250, so they are NOT the shipped state and must not be the
   owner's judgement package.
3. Zixxtrixx 16-subject CRC gate from a self-built baseline.
4. `tovideo.py` -> `assemble.py` -> archive the outgoing generation -> deploy.

Two faults found today worth carrying out of this run:
* **A full disk imitates a rendering bug.** `cannot open .../0038.rgb`.
* **A backslash-t inside a string became a literal TAB** in three separate
  tools today, including `deploy.ps1`'s checkmedia caller — where it would have
  refused every publish while blaming the media. Look at the BYTES.

### Done while the bank rendered (nothing in its closure)

* **Archived pass 10 BEFORE anything overwrote it** — 22 clips + posters to
  `archive-pass10-u02-*`, **decode-verified with ffprobe, 0 failures of 22**.
* Pass 10 archive entry + `archive_note` EIGHT -> NINE generations.
* Blurb rewritten for pass 11, **after the last constant moved**.
* `MANAFOLD-INDEX` entries for the pass-11 findings and the particle lab.
* **Un-stranded the particle lab's report**: its findings doc and 14 plates
  cherry-picked to `main`; the driver and its experimental constants stay on
  `manafold-p11-particle-lab`. The brief's "do not merge" was right about the
  CODE and wrong about the REPORT.
* Blast radius checked by diff: only `manafold_*` and `zhao_reel.cpp` changed,
  and every `zhao_reel.cpp` hunk is inside a u02 path or an inert getenv.
  **No Zixxtrixx or engine source touched.** The CRC gate is still owed as the
  proof.
* Lane audit: this lane shows **no** NOT-ON-ANY-REMOTE and **no** UNPUSHED.
* Looked at the lab's `05-THE-EDGE-native.png` myself rather than relaying it
  unseen: the radius change is real and MODERATE at native — the particles read
  as beads on a path instead of a blur. Worth the owner's eye, not worth
  shipping over his head.

### Still owed

F.5 package (staged in `f5.sh`), Zixxtrixx CRC, encode + assemble + deploy.

### Close-out gates

* **Zixxtrixx CRC: 16 of 16 identical, 0 differing**, against a baseline built
  by me from `3b2b8b91` in its own git worktree and its own output tree.
  Committed as `probes/zixxcrc.sh` so the numbers are reproducible.
* **Probe against the shipped build: every gate green** — closure sweep 989 /
  clip bank 1072 (gate 1120), C.1 tip travel 255 mm LIVE, mist follow OK, all
  7 shipping mist rows attached with `parked` at exactly 0, ladder continuity
  OK, E.2 depth ordering OK with 54 mm margin and the star a form at 547 pm.
* **Six known-bad legs, six failures**, every fault injected into the SHIPPED
  path. Transcripts committed.
* **Pass-10 archive taken BEFORE the encode overwrote anything**, 22 clips +
  posters, ffprobe-verified, 0 failures.
* Encode of the 22-clip bank running; spot-checked decodes match their source
  frame counts exactly (420 / 600 / 180), so CPU contention with the CRC
  renders did not damage them.

### Note for the next pass

I ran the CRC renders CONCURRENTLY with the encode. It roughly halved the speed
of both and it was still the right call over idling — but the earlier
`fog1200` render that died silently at frame 198 was almost certainly the same
contention, so treat a render that stops mid-write with no error as a
resource problem first and a renderer problem second.
