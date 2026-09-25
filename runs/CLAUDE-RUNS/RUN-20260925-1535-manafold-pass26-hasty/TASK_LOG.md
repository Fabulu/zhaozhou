# Task Log: RUN-20260925-1535 - [Describe objective here]

**Created:** 2026-09-25 15:35 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260925-1535-manafold-pass26-hasty/

---

## Objective


Manafold pass 26 (small) from Owner Direction 27: make HASTY actually look hasty - in speed and in facial expression. The owner closed the other three flagged items: Crackles rear is fine, Hovers front ball is fine, and Hastys loop hitch is accepted because the creature traverses the screen.

---

## Progress Timeline

### 2026-09-25 15:35 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260925-1535
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

### 2026-09-25 15:35 - Started
- Branches `manafold-pass26` created in both repos from the production-verified pass-25 mains (Zhaozhou `f66d107c`, Upheaval `7cb6c51c`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-27-2026-09-25.md`.
- Pass 25 was the creature's planned final pass; the owner reopened one clip only. Scope is Hasty and nothing else.
- One Opus implementer, then a bounded independent review that publishes on PASS. No Qwen (reserved); GPT/Codex quota reset 2026-09-25 - check before assuming it is still exhausted.

---

### 2026-09-25 ~15:40 - Diagnosis BEFORE touching a value

Built the production renderer, rendered pass-25 Hasty, looked at all 240 frames.
Findings in `P26-NOTES/FINDINGS-01-why-not-hasty.md`. Four causes, three of them one
leftover: **Direction 12 deleted the traverse and left the camera's traverse
compensation in place**, so all the screen motion is a pan carrying the ground
with it. Measured relative motion **+0.066 px/frame** -- zero speed cue.

### 2026-09-25 ~15:45 - New instrument: tools/reel/screenmotion.py

No existing tool differences creature motion against ground motion, which is the
only way to see a pan masquerading as speed. Six selftest legs. Three faults
found in the instrument/fixtures before it was trusted:
- the first pan-only fixture slid the sampling window the wrong way (fixture
  wrong, tool right);
- the integer correlator returned a confident `0.000` on 239 consecutive frames
  for a ~0.68 px/frame pan -- a detector whose quantum exceeds the effect. Fixed
  with `--lag`; leg F asserts the CORRECT behaviour, not the bug;
- the chroma mask's first threshold scored a frame with **no creature** at
  29,719 px. `screenmotion.py absent` is the committed known-negative check.

### 2026-09-25 ~16:00-16:30 - The ladders

Rungs and what each looked like: `P26-NOTES/FINDINGS-02-ladders.md`. Staging took
five rungs because three separate things were wrong -- the follow must scale
with `cam_k`; the follow was clocked by the FRAME INDEX not the animation phase
(the creature vanished at the seam); and **the traverse ran 45 degrees into the
lens** because the u02 camera is a fixed three-quarter. Chosen by eye:
cadence 13, surge 1400, cam_k 280000, follow 820, eye 880/430.

### 2026-09-25 ~16:35 - Two gates red, both right

- `n-meyesize`: the expression-slot list was a hardcode the GATE held alone.
  Moved to `u02::eye_expression_slot()`, read by the gate and the clips.
- `n-mqa` Q3: the 135 mm root-continuity ceiling refused the bob at 165 mm.
  **No bound relaxed** -- the amplitude moved to 140 (150 passes by 0.3 mm and
  was rejected for being that close). Re-looked: indistinguishable.

Killed the first matrix run once its result was stale. It left an orphan
`manafold-boltgate` AND a live parent shell that kept spawning more; identified
both by full command line, killed only mine, verified 0 remaining. The other
lane's Quartus `jtagserver.exe` was identified and **left alone**.

### 2026-09-25 ~16:45 - Source committed and pushed

`78d8fde9` on `manafold-pass26`. Evidence follows once the re-run matrix lands.

**WHERE I AM / NEXT STEP (written before reading the matrix):** the full gate
matrix is re-running on the corrected tree. Everything else is done -- report
written, looks curated, byte-identity and seam receipts taken. Next: read the
matrix tally, fold it into `P26-IMPLEMENTATION.md` section 8, commit the
evidence, push. Do NOT render the bank for media, encode, merge or deploy --
the coordinator sends the review/publish packet.

### 2026-09-25 ~17:30 - A face check that CUT instead of leaving

Found by reading the curve against its own comment and printing it, not from a
frame. `tri = (t <= half) ? t : span - t` never reaches zero coming down, so
every check ended at 25-33% of full and cut -- a one-key step in eye size, gaze,
lid AND brow at once, because all four ride the single curve. The comment above
it claimed the opposite. Replaced with a symmetric triangle; all windows now
odd-span so the peak is sampled. Committed `bfb22a65`.

### 2026-09-25 ~18:10 - The inherited matrix's identity floors were ALREADY broken

`e-p24off` and `e-p23off` red on **hover and inspect** -- subjects this pass
never touched -- while crackle matched exactly. Chased in
`P26-NOTES/FINDINGS-04-inherited-matrix-fault.md`:

- the hurry knob does not leak into hover (0/600 frames);
- the pass-25 reference binary and the pass-26 binary render the floor
  identically (0/600);
- the renderer is deterministic (hover twice: `0x89EBA648`, 0/600);
- so the **pass-25 binary itself** misses the receipt.

Cause: `kBackBallDampClipPm` is 700 on **slot 0 (hover AND inspect)** and 0 on
slot 23, and the floors have **no back-ball off-flag**. The pass-25 matrix
receipt was committed at `82218e2a`; the back-ball packet landed after it
(`af8c97c1`/`b3c5760e`, 1,134 insertions) and **the full matrix was never
re-run**. Proof: floor + `ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0` on the pass-25
binary gives `0x8EDC6DE3` / `0xA7972F35` -- the receipt exactly.

Repaired with `$BBOFF` plus `e-backball-live` as its positive control.

**And one self-inflicted:** that matrix run died on a bash syntax error because
I edited the script WHILE IT WAS EXECUTING -- bash parses incrementally from a
byte offset. The live-tree trap applied to a shell script. The re-run is
launched from a frozen copy in `.tmp/`.

**WHERE I AM / NEXT STEP:** matrix re-running from `.tmp/p26/matrix-frozen.sh`.
Everything else is complete. Next: read the tally, fold it into
`P26-IMPLEMENTATION.md` section 8, commit evidence, push. Do NOT encode, merge
or deploy -- the coordinator sends the review/publish packet.

### 2026-09-25 ~20:30 - Matrix 275/275, zero failures

Two more faults surfaced and were repaired before the clean run:

- **`e-p25off` pointed at a stale file.** `P25-RECEIPTS/crcs-ship.txt` was
  written by the pass-25 MATRIX (commit `82218e2a`) and predates the back-ball
  packet, so it records hover `0x8124751D` where the pass-25 tree renders
  `0x89EBA648`. **Pass 25's shipped bank is not in doubt:** its own back-ball
  receipt, the independent reviewer's receipt, and a rebuild of `f66d107c` here
  all agree on all four checked subjects. One stale file, not a stale creature.
  The leg now uses the reviewer's receipt (independent-build provenance).
- **`e-backball-live` could not read its own baseline.** It ran before
  `bank e-ship` wrote `e-ship.crc`, `diff` errored to stderr, and the empty
  result reported `changed=[]` -- **which is exactly what a dead flag looks
  like.** A positive control that cannot read its baseline fails in the shape of
  the finding it exists to rule out. Ordering fixed.

Final: **total 275 pass 275 fail 0.** Every pass-26 control fired, every bad
value refused, both repaired floors exact, the back-ball positive control
changing exactly hover and inspect, and the exact-off control byte-identical.

### 2026-09-25 ~20:45 - Closed

Source `78d8fde9` + `bfb22a65`, evidence to follow. NOT encoding, merging or
deploying -- the coordinator sends the review/publish packet.
