# Task Log: RUN-20260906-0524 - [Describe objective here]

**Created:** 2026-09-06 05:24 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0524-manafold-p7-by-eye-review/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 05:24 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0524
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

## Pass 7 BY-EYE REVIEW log

### Lane and calibration
- Own lane `manafold-p7-review/{zhaozhou,Upheaval}`, cloned then repointed at
  GitHub and hard-reset to `origin/main`: **zhaozhou `6f840909`, Upheaval
  `097ffbe`** -- the exact published tips, confirmed by `git fetch`.
- Built my own binary: `bash tools/reel/build-direct.sh --output <lane>/build cel`,
  **BUILD_RC=0 read from the recorded exit code**, not from a pipeline tail.
  `zhao-reel-cel.exe` 2,739,759 B -- the same size the publish run recorded.
- Judging the SHIPPED artefacts (the committed webms that production serves,
  `cmp`-verified by the publish run against production) at native 384x240.

### Instruments written, and PROVED FAILABLE before use
1. **`tools/reel/webm2rgb.py` -- COMMITTED this time.** The publish run wrote
   this and left it in its run folder, where CLAUDE.md says durable things get
   orphaned; it was gone. Selftest: rejects a 2 kB stub (the documented
   `ffprobe` hole), round-trips red as (254,0,0) and blue as (0,0,255) so a
   channel-rotating decoder cannot pass, and distinguishes the two.
2. **`plates.py crop`** -- added to the committed plate builder rather than
   hand-rolled. Out-of-range boxes RAISE; a silently clamped crop would move
   what is measured between frames (gate item 17). Proved: rejects 370,0,40,40
   on a 384-wide frame.
3. **`eyesheet.py`** -- per-frame eye locator + contact sheet. Selftest proves
   it does NOT match the mauve sky (the project's recorded mask fault), DOES
   find a planted lens at the right coordinates, and returns *no centre* rather
   than a default on a creature-less frame; misses are drawn MAGENTA and
   counted, never silently centred.

### Frame counts cross-check
Decoded clip lengths match the publish run's table exactly (hover/inspect 600,
channel 420, taunt 280, rest 400, curious 180, pirouette 240, startle 160,
taunt2 240), which independently re-confirms "all 16 clip lengths identical".

### In progress (written down BEFORE reading the antenna render)
Was: judging Direction 5 §2a by eye on `manafold-antenna-fixed`, rendered from
my own build. Next after that: the smear-plane colour attribution
(`manafold-rest` minus `manafold-fogprobe-mana` = the smear's own pixels, the
one thing the committed ablation pair CANNOT see because it has the smear off
on both sides), then eyes-touch/clip by eye, then write up.

### Findings so far (each stated as verified-by-eye or measured)
* Claim 1 star-reads-as-star: CONFIRMED at native, strongest on `curious`.
* Claim 2 `channel`'s white spike: CONFIRMED GONE at native, f231.
* Claim 4 smear on all clips: CONFIRMED by 8-clip A/B against the pass-6 archive.
* Claim 7 black notches: measured INSIDE each eye, per named part -- ~0.1 px
  per frame in both passes. Not a read fault. My first metric said the opposite
  because a dilated lens mask swallowed the body's own ink outline.
* Mana: the "white steam" verdict does NOT survive re-measurement (below).
