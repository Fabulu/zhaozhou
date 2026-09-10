# Task Log: RUN-20260910-0607 - [Describe objective here]

**Created:** 2026-09-10 06:07 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260910-0607-p15-lane-fx3-fog-not-bleach/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-10 06:07 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260910-0607
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

## LANE-FX-3 — finish the fog, the publish is held on it

### 06:07 — ARRIVAL, and the brief's premise is WRONG

My brief says the FX-2 lane was *"interrupted before authoring anything — the
constants are untouched."* **They are not.** The lane's working tree carried
**215 lines of authored `manafold_fx.h`**, the full pick, plus the `zhao_reel.cpp`
env knob and a 204-line TASK_LOG entry recording how it got there. Two commits
(`5a621e43`, `c7d6be79`) had already landed the `rungsweep.py` `--sep` fix and the
**re-aimed `manafold_shellgate.cpp`**.

It did not stop before authoring. **It stopped after authoring and before
PROVING**, in the middle of a sequence it had written down:

    edit constants -> --clean build to a NEW dir -> byte-identity against the
    judged rung -> mshell + --selftest -> Zixxtrixx bitident -> plates -> findings

Everything left of the arrow was done. Everything right of it was not. That is
the lane I am finishing, and it is a materially different job from the one I was
briefed for — **had I obeyed the brief's premise and started from "constants
untouched", I would have re-derived a pick that already existed and thrown away
two ladders.** (10-GATE item 38: a brief is an instrument and it can lie like
one. Item 38.3: an agent that contradicts its brief is doing its job.)

**Handling.** `git fetch && git reset --hard origin/main` in both repos as
instructed — zhaozhou was already *exactly* at `origin/main` by commit, so the
reset could only discard the uncommitted authored work; Upheaval was 1 behind and
took `9f1a028` (the QA lane). I saved the working diff **before** the reset,
verified `git apply --check`, reset, and re-applied. Net tree identical, but the
base is now provably `origin/main` rather than assumed to be.

**Load.** Full `Get-CimInstance` census by command line before the first build:
**no `quartus*`, no `zhao-reel-cel`, no `ffmpeg`, no compilers.** The machine is
idle. C: has **210 GB free** — the previous lane died when the disk hit zero, and
that is resolved. Nothing killed; no `taskkill /IM` in this run.

### 06:15 — I LOOKED AT THE THREE PLATES BEFORE ACCEPTING THE PICK

A pick I have not seen is not a pick I can ship, whoever authored it.

* **A01 (ablation, `inspect` f0300, 3x)** — decisive. P14-published and
  P15-SHELL-OFF carry the *same* round deep-plum-to-magenta terminator; P15
  SHIPPED is a chalky marble with the form erased. **The bleach is the shell**,
  one variable, one binary. The review's unrun leg is run and it lands.
* **A02 (profile ladder, day)** — the profile fix restores the form completely.
  At 200 the core is indistinguishable from the ablated tile. **And at every
  surviving rung the rim is a chalky WHITE band** — frost on the animal, not the
  animal's gas. The profile alone was never going to be enough.
* **A03 (tint ladder, `channel` f0180, 4x, night)** — one profile, one alpha,
  only the colour differing. `mana` is plainly gas where `white` is dust on the
  lens. The bracket fails in both directions (`rose` -> wash, `deep` -> invisible).

**I agree with the diagnosis and with five of the six values.** ⚠ **I do not yet
agree with `kShellRiseGamma = 1050`, and I am saying so before I render.** On
A03's 4x night crop, `RIM MANA g1600` reads as a *more legible* band of gas than
`PICK MANA g1050`, which is softer and more diffuse. The lane's argument for 1050
is that it carries more gas *past the ink line* — but A03 is an interior lower-body
crop and **barely shows the exterior**, so the plate does not contain the evidence
its own pick rests on. And "I still don't see it" is what the owner has said about
this shell for four passes: softer is the expensive direction to be wrong in.

**That is the question my ladder has to answer, at NATIVE**, where a 4x crop
cannot flatter a subtle effect into visibility.

### 06:40 — BYTE-IDENTICAL, and that single comparison closed four questions

    fx3a  (NEW constants, NO env)          channel f0180
    fx2a  (OLD constants + the PICK rung)  channel f0180
    -> afc7ba16260f91ce51bf52a4d2bd2df3   BOTH.  BYTE IDENTICAL.

Two different binaries (`4763d62d` and `a5f028ff`), arriving from opposite
directions, emitting the same bytes. It proves, in one `cmp`:

1. all six constant edits landed exactly as authored;
2. `--clean` really did rebuild the header — this is the stale-binary trap's
   only airtight answer, and it cannot be fooled the way an mtime can;
3. **the thing that ships is the thing that was judged** — the shipped tuple is
   `A03`'s `PICK MANA G1050` tile, not an interpolation between two rungs;
4. `kShellFogDepthMinPx 5 -> 2` is **inert** at this subject's body size.
   Proven, as the previous lane said it would be, rather than asserted.

Both my ladders also carry **4 distinct md5s per backdrop** — the rungs are
genuinely different pictures, which is the failure the `--sep` fix exists to
prevent and which no ladder before it could rule out.

### 06:52 — THE GATE WAS RED ON THE CONSTANTS IT WAS RE-AIMED TO BLESS

Ran `mshell` expecting to confirm the previous lane's green. **Check 3 failed** —
and it failed in *every* `--selftest` leg too, i.e. unconditionally.

    [FAIL] 3 the fog is THICKEST INWARD
           densest at 3 px INSIDE the line ... (threshold was 4)

The chain, which no counter anywhere would have shown:

    check 6 went red  ->  kShellFogDepthMinPx 5 -> 2  ->  ann_px on the
    radius-24 fixture falls 5 -> 4  ->  sampled peak (the ink px is skipped)
    falls 4 -> 3  ->  CHECK 3 GOES RED.

**Fixing one check broke another, and the run that made the fix ended before the
gate could say so.** The previous lane's last log line is the floor edit and its
promise that the change would be "proven by byte-identity rather than asserted".
That proof is real and I reproduced it — and **it is evidence about the pixels,
which is a different question from the gate.** Two instruments, one re-run.

Two faults, both fixed:

* **the FIXTURE.** radius 24 makes a 180 pm annulus 4 px wide, so a single
  pixel of ink-skip decides the verdict. That is the identical "threshold
  coincides with the value under test" coin toss the previous lane diagnosed and
  repaired **in check 7, the check immediately beside it**, without sweeping its
  neighbour for the same shape. Check 3 now uses the same radius-40 fixture.
* **the THRESHOLD'S PROVENANCE.** `kShellOutReachMinPx + 2` is a floor on the
  OUTWARD skirt and has nothing to do with where the peak belongs — a
  plausible-looking number standing in for an argument. The falsifiable content
  of his sentence is that the peak is STRICTLY INSIDE the line, so: 2 px, clear
  of the ink, **named**, and the margin printed.

Checks 3 and 7 now share one fixture and form a real **two-sided window** on a
radius-40 body — 3: not too shallow (>=2), 7: not too deep (<13). The shipped
values land at **5**, mid-window, margins 3 and 8, on neither edge. Both remain
SHAPE claims, so `10-GATE` §0 holds. And the two checks, written independently,
now agree on the same peak from the same scene — a free cross-check.

**All 7 PASS on the shipped constants; all 7 still fail under `--selftest`.**

### 07:00 — `--regression`: the gate against the build a human refused

`--selftest` ablates ONE knob per leg, which proves each check is wired to
something. It does **not** prove the gate as a whole sees the fault the reviewer
saw with his eyes — a harder and more important claim, and the one the previous
lane's green was wrongly quoted for.

So `manafold-shellgate --regression` restores **all six pass-15 values at once**
and requires the gate to go RED, with **inverted polarity** (rc 0 = caught it).
It writes only the `g_u02_shell_*` overrides — the same mutable globals the
reel's env knobs drive — so no production constant is edited and there is no
live-tree hazard. It is the committed-mutant idea applied to a gate.

    shellgate --regression: CAUGHT IT   rc=0
      [FAIL] 2 the CORE KEEPS ITS OWN PIGMENT -- centre 177 vs unpainted 120
      [FAIL] 7 the fog is an OUTER LAYER -- peak 17 px in, must be under 13

That is checklist 43's question answered on the record instead of in prose.
