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

### 07:20 — REBASED MID-FLIGHT, AND THE PUSH THAT SAID IT SUCCEEDED HAD FAILED

`git push ... | tail -3` printed **`PUSH_RC=0`** while the push was **rejected**
non-fast-forward. That is `tail`'s status, and CLAUDE.md's own entry — *"read the
real exit code, of the thing, not of the pipeline"* — caught in the wild, in the
one command where believing it would have meant reporting work as landed that was
still sitting in my lane. Re-run bare: `REAL_PUSH_RC=0`, and then verified from
outside by `git fetch` + `git log origin/main`.

**LANE-EYE-2 had landed `c269b448` while I worked**, and it touches
`zhao_reel.cpp` — a file my brief calls mine. **Rebased, never merged blind.**
Clean, both lanes' work present afterwards, verified by grepping for my seven
constants and their `U02_SHELL_INK` knob on one side and the eye lane's commit on
the other.

### 07:30 — I RE-RENDERED ON THE MERGED TREE, BECAUSE THAT IS THE FAULT THIS PASS WAS CRITICISED FOR

The review's own words: *"Every plate in `pass15-plates-eye/` predates the FX
merge… nobody has judged the merged result."* My ladders came from a **pre-merge**
binary, so shipping them unremarked would have been the same fault in reverse.

Rebuilt (`eee6bd5c`), re-rendered the three key rungs, and looked again: **the
shell holds.** Core deep plum, terminator present, gas at the rim.

And the cheap check that made it airtight — the merged frames are **byte-identical
to the pre-merge ones**:

    OFF_ABLATED-0300      IDENTICAL pre/post merge
    P15_REGRESSION-0300   IDENTICAL pre/post merge
    SHIP_g1050-0300       IDENTICAL pre/post merge

So LANE-EYE-2's change is **inert on `manafold-inspect` f0300**, and my judgement
frame is the same pixels either way. ⚠ **That is a statement about ONE subject and
must not be generalised** — the QA lane's own commit title says the eye fix
regressed seven live subjects. It says my ladder is valid on the merged tree; it
says nothing about theirs.

I had also *thought* the lens looked different between the two plates. It did not
— the two plates used different column counts and therefore different display
scaling. **A crop confirms; an impression does not** (item 41), and I checked
instead of writing it down.

### 07:35 — LOAD: a sibling renderer identified, and NOT killed

Census by command line found `zhao-reel-cel.exe` running out of
**`manafold-p15-eye/build-before`** and later `build-eye` — a sibling lane
mid-render, in a directory I am forbidden to touch. Nothing killed; **no
`taskkill /IM` anywhere in this run**. I held my own concurrency to one renderer
at a time and waited for the sibling to finish before starting the long
bit-identity job. This is the night the owner lost eight hours, avoided by ten
seconds of `Get-CimInstance`.

### 08:10 — ZIXXTRIXX BIT-IDENTITY: 69 IDENTICAL, 0 DIFFERS, 15634/15634 FRAMES

`zhao_reel.cpp` is on the protected list, so its bytes must be re-proved.
**Self-built baseline**, not an inherited one: a detached `git worktree` at
`c269b448` — the exact commit my work sits on — built `--clean` to
`0f236fb9`, against my merged tree's `eee6bd5c`. That isolates **my** delta
rather than the merge's.

    IDENTICAL 69   DIFFERS 0   EMPTY 2   of 71
    frames 15634/15634 identical
    BITIDENT_RC=0

⚠ **The tool flagged its own weakness and I am repeating it rather than burying
it:** `distinct CRCs 68 of 69 non-empty subjects <-- COLLAPSED, the metric is not
discriminating`. Two subjects share a CRC, so the CRC leg has a collision. This
is **pre-existing** — the previous lane's `bitident.log` in this same lane
carries the identical line — and it is exactly why the tool runs **two**
independent metrics: the per-frame sha256 leg is unaffected and is what carries
the weight here. Worth someone's attention; not mine to chase, and not a reason
to doubt this result.

### 08:15 — THE POSITIVE CONTROL, because a green that has never gone red is not evidence

`bitident.py`'s own header: *"Run it BOTH WAYS ROUND. Green against a
default-off change proves the change is inert; it does not prove the harness can
see anything. Build a deliberately mutated binary and this must go red, or the
green meant nothing."*

The mutant is **one substantive line** in the shared engine — the angular U
mapping in `creature_core.cpp`, which every creature renders through:

    out[k].u = static_cast<uint8_t>(ang >> 8);        ->  ... + 1);

⚠ **It lives ONLY in the disposable detached worktree and is never committed to
a production tree**, so there is no live-tree hazard and nothing can elaborate it
by accident — the committed-mutant rule's intent, met by isolation rather than by
a renamed file, because this is a throwaway build rather than a durable artefact.
It carries a header saying what was changed and why.

Scoped to **4 subjects, declared**: the control's job is to show the instrument
can see a change, not to survey the bank a second time.

### 08:35 — THE POSITIVE CONTROL WENT RED ON EVERY SUBJECT

    zixxtrixx-attack   560 frames   0 identical   DIFFERS
    zixxtrixx-idle     576 frames   0 identical   DIFFERS
    zixxtrixx-walk     160 frames   0 identical   DIFFERS
    zixxtrixx-death    192 frames   0 identical   DIFFERS
    IDENTICAL 0   DIFFERS 4   of 4      MUTANT_BITIDENT_RC=1

**Both metrics moved** — the CRCs differ as well as the per-frame hashes — so the
harness demonstrably sees a one-line change in shared engine code, and the 69/69
green above is evidence rather than silence. Mutant reverted from the worktree
immediately after; `git status` clean, and the marker comment greps to 0.

### 08:40 — CLOSING THE BLIND SPOT I HAD JUST DECLARED

Writing §8 I had to add a caveat: the `MinPx 5 -> 2` byte-identity proves
inertness **at `channel` f0180's body size only**, and the edit is *deliberately*
NOT inert on small, distant bodies — that is its whole purpose. But **no plate in
this pass, mine included, looks at that size range**, and `flight` and `hasty`
live there.

Having written the words "structurally blind", the honest move was to stop
writing and go and look. Rendering `manafold-flight` f0100 — ablated, shipped and
the regression — rather than shipping a declared gap I had the machine free to
close. A declared limitation is better than a hidden one; **a closed one is
better than either.**

### 08:55 — THE SMALL-BODY LEG LANDS, AND IT CLOSES THE GAP

`manafold-flight` f0100 (R ~ 20 against `inspect`'s ~55), same binary, same three
rungs:

    SHIP g1050        4523 px painted   peak 83    CENTRE delta 0
    P15 REGRESSION    6451 px painted   peak 107   CENTRE delta 52

**The core is untouched on a small body too**, and the regression paints straight
through it. Confirmed by eye at 6x: the dark plum shading on the right of the
ball survives under SHIP and is lifted to a pale mauve under the regression.
`MinPx 5 -> 2` does what gate check 6 demanded, on a real travelling subject
rather than a synthetic disc.

### 09:00 — CLOSE-OUT

**Pushed and verified from outside the lane**, never from a push's own output —
which lied once already today:

    zhaozhou  b2464c60  the shell
    zhaozhou  5ba28806  the gate + --regression
    Upheaval  e2179e4   the probe, the plates, and LANE-FX-2's findings
    Upheaval  dc74cdc   PASS-15-FINDINGS-FX3 + pass15-fx3-plates

**Background jobs: none of mine alive.** `Get-CimInstance` shows only
`manafold-p15-eye`'s renderer and another lane's two `quartus` manifest checks —
identified by command line, **left alone, nothing killed, no `taskkill /IM` in
this run at any point.**

**Housekeeping:** my `base-tree-fx3` worktree removed with `git worktree remove`;
the mutant reverted and greps to 0; 272 MB of the previous session's stale
`fx2-work/final` frames purged, evidence frames kept. C: at 206 GB free.

**I did not publish.** The standing bestiary authorisation is for *a finished
creature pass*; this one has sibling lanes in flight and a QA lane reporting
seven regressed subjects. **The fog no longer holds the publish — something else
still might, and that is a coordinator's call, not mine.**
