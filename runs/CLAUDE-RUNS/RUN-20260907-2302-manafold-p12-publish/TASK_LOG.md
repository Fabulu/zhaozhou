# Task Log: RUN-20260907-2302 - [Describe objective here]

**Created:** 2026-09-07 23:02 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260907-2302-manafold-p12-publish/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-07 23:02 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260907-2302
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

## Objective (overrides the template placeholder)

RENDER AND PUBLISH Manafold pass 12 (creature 02). Wave-1 implementers A and B
have merged to `origin/main`; the live site still serves **pass 11**. This is a
**deliberate partial publish, mid-plan** — the coordinator overrode the plan's
single end-of-pass publish because Wave 2 (eyes, theatrical clips) builds
directly on the round body, the bounce and the nodules, and the owner needs to
see those before more is authored on top. **The page must say so.**

## Lane

`C:\programmieren\zencrifice\manafold-p12-pub\{zhaozhou,Upheaval}` — clean
clones of `origin/main`. `zhaozhou` @ `d820b574`, `Upheaval` @ `a9e532e`.
Nothing outside this lane is touched.

## Log

### 23:02 — run opened, lane cloned
Both repos cloned `--no-hardlinks` from the local mirrors, remotes repointed at
GitHub, `git fetch origin main`, `checkout -B main origin/main`.
- zhaozhou `d820b574` "kFoldShapeCount reaches the three new figures…"
- Upheaval `a9e532e` "Pass 12: lane B findings and the remaining plates"

### 23:05 — build, from the clean clone
`bash tools/reel/build-direct.sh --output <lane>/build cel`
**`BUILD_RC=0`**, read from the recorded exit code and not from a pipeline's
`tail` (CLAUDE.md: "Read the build's exit code, not the pipeline's").
`build/bin/zhao-reel-cel.exe`, 2,817,019 B, md5 `925f58d8d37f7dc1118adaacebeff115`
(recorded to `render-p12-binary.md5` BEFORE the render).

### 23:07 — ARCHIVE OF THE OUTGOING PASS-11 GENERATION, before any encode
This is step 4 done first on purpose: the encode overwrites `manafold-*.webm`
in place, and pass 11 was caught nearly shipping stale clips.

47 files copied `manafold-<clip>.{webm,png}` -> `archive-pass11-u02-<clip>.{webm,png}`:
**22 webm + 25 png**. The three extra pngs are the eye/mana comparison sheets,
which is exactly what the `archive-pass10-u02-*` precedent did (22 webm / 25 png).
The live clip set and pass 10's archived clip set are name-for-name identical.
Copied with `cp -n` so an existing archive could never be silently clobbered.
Byte-identity spot-checked by sha256 on `hover`, `startle`, `crackle`: **3/3 OK**.

There was NO pass-11 archive group on the page before this — the archive tabs
ran 10, 9, 7, 6, 5, 4, 3, 2, 1. Pass 11 was the live bank and had never been
preserved. It is now.

### 23:08 — rig verified by READING the source, not inferred
`tools/reel/zhao_reel.cpp:5407` `subject_u02_clip(...)`, line 5429:
`s.creature_moving_light = true;` — every Manafold clip subject raises the
many-colour moving rig, which gates the per-clip `kU02Sun*` suns OFF. That is
the shipping rig, and it is why the old "under the hover sun" captions were
false.

### 23:09 — render: ONE binary invocation, 22 subjects named explicitly
    ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
      build/bin/zhao-reel-cel.exe <lane>/Upheaval/website/scratch-reel \
      manafold-hover manafold-inspect manafold-drift manafold-channel \
      manafold-curious manafold-startle manafold-rest manafold-pirouette \
      manafold-hasty manafold-fall manafold-hit manafold-taunt \
      manafold-taunt2 manafold-trick manafold-damage manafold-crackle \
      manafold-mana-aqua manafold-mana-cyan manafold-mana-blue \
      manafold-mana-green manafold-mana-boil manafold-mana-stack

22 = the 16 motion clips + the 6-variant mana menu. Named explicitly because a
bare output-dir invocation renders the WHOLE library into the cwd (gotcha §8),
and `--help` would be taken as an output directory.

### 23:04 — OWNER DIRECTION LANDED MID-RUN (Direction 9 §15)
My first push was rejected: `origin/main` had moved under me. The new commit
was `0773756` **"Direction 9.15: the lasso returns -- thrown by the antennae,
made of mana"**, which **REVERSES §10.3** — where the lasso had been ruled
*"removed… not coming back… no future pass rebuilds it"* earlier the same day.

This is exactly the failure CLAUDE.md warns about ("instructions are not
delivered until they are read"), caught only because a push conflicted. Read it
before continuing. It changes what the **Lasso** tab is allowed to claim, and
that tab's caption now carries the reversal rather than a stale closure.

Rebased, pushed, landing verified with `git branch -r --contains`.

### 23:2x — the pass-11 archive group added to the manifest
`add_pass11_archive.py`, modelled on the `archive-pass10-u02-*` group:
22 items, `archive_generation: "Pass 11 — 2026-09-07"`. The diff is
**98 insertions / 0 deletions** — a clean insert, so the reserialisation
disturbed nothing else in a 262 KB file. `assemble.py` accepts it; Manafold
goes to **10 archived generations** against `MAX_ARCHIVE_GENERATIONS = 19`.
Committed and pushed (`9f99671`), landing verified.

### 23:3x — CAPTIONS, checked against the SOURCE and not against the prose
A research pass read D9, the plan, both implementers' findings and the D4
diagnosis, then re-read every load-bearing constant in `tools/reel/*.h`.
**Two places where the pass's own documents disagree with the shipped code**,
resolved in the code's favour:

1. `PASS-12-FINDINGS-B` §8.1 says the fold picker *"still selects from the
   first six"*. **Stale.** `manafold_clips.h` `kFoldShapeCount = 9` landed in
   `d820b574` — the HEAD commit, after lane B closed. `manafold_fx.h:1296`
   still carries the old "NOT WIRED YET" hand-off comment. Captions say NINE.
2. The plan's PROTECTED list carries the per-clip `kU02Sun*` as if they lit
   something. **Dormant since pass 6.** No caption claims a sun; the hover note
   says thirteen named moods across sixteen clips, lighting nothing.

**Two claims I deliberately WEAKENED from how the work was handed to me:**
* not *"one mana lighting bank-wide"* — a clip must return its lights to where
  it started or the loop pops, and no single rate divides evenly into every
  clip length (gcd 4 frames), so each takes the whole number of turns NEAREST
  `channel`'s 420-frame model. **Near-equal, not equal**, and the page says so.
* not *"the rear hinge now connects"* — the junction still travels ~298 mm.
  Churn is down 62–73%; whether it READS as connected is the owner's eye.

Blurb + 14 tabs rewritten; **19 lines in, 19 out**. Committed `0e81267`,
pushed, landing verified.

### NEXT STEPS (written down BEFORE reading the render's result)
1. render finishes -> start the encode in background
2. build `mnodule` + `mprobe` (safe once the reel exe is unlocked) and run
   the committed nodule gate and the clearance/closure probe
3. native-384x240 comparison plates, pass 11 vs pass 12, via the COMMITTED
   `plates.py` / `rgbframe.py` / `webm2rgb.py` — no fifth hand-rolled reader
4. **LOOK**, then encode-verify, assemble, deploy
5. re-verify the hover/inspect byte-identity claim I put in the Inspect
   caption — it is a pass-7 number and I asserted a pass-12 one

## LOOKING AT IT — and the first read was WRONG

Method: the outgoing pass-11 clip exists only as an encoded webm, so the
COMMITTED `webm2rgb.py` lifts frames back into the reel's own `.rgb`, and the
COMMITTED `plates.py` on the COMMITTED `rgbframe.py` reader builds every
comparison. No fifth hand-rolled frame reader — `rgbframe.py`'s own docstring
records that two of the four confidently-wrong diagnostics on this creature
were bad readers.

### The mistake, recorded because it is the project's own law biting me
At native and at 3x, `hover` f160 pass 12 shows a **thick black rim** the
pass-11 frame does not have. I read that as the shell having swallowed the ink
line and was ready to stop the publish over it.

**That read was wrong, and it was wrong in the textbook way: I compared against
the wrong reference.** Pass 11 is not the reference — it is the generation
whose shell was reverted to "a whisper", and lane B's own ladder plate labels
it *"PASS 11 shipped (too faint)"*. The reference is **v1, the concept**, whose
top panel in `B4-shell-final-vs-v1-and-pass11.png` has exactly this heavy dark
rim. Pass 12 is the RESTORATION that Direction 9 §7/§14 asked for. I was
judging a deliberate, owner-requested change as damage because the previous
frame looked different. **Compare like with like, or do not compare.**

### What the rim actually is, measured on the comparison side
A row through `hover` f160 at y=153: the dark band on the left rim is
**x=139..142 — exactly 4 px**, which is exactly the `ink_width=4` the reel
itself reports for this frame. It is the INK, at the width the shared
`cel_main_ink_width` function gives for this projected radius. The round body
projects LARGER than pass 11's teardrop, so a wider line is by construction,
not by accident. At 3x a 4 px line is 12 px, which is what made it look fat.

At **8x on the rim** (`LOOK-rim2-8x.png`), pass 11 has **no outline at all** —
the body simply ends against the sky. Pass 12 has a **solid, continuous black
ink line with pale gas on both sides of it**: a soft aura outside, a lighter
gassy rim inside before the body's pink. That is lane B's alpha-440 rung, and
the ink is plainly readable. **Acceptance met.**

### ⚠ AN INSTRUMENT THAT HAS GONE QUIET, and it will lie by silence next pass
`inkwidth.py` on the pass-12 `hover` f160 finds **18 ink pixels**, on a
creature whose outline is plainly hundreds of pixels long. Its mask matches the
EXACT quantised triples (25,24,25)/(25,24,16), and **the shell composites over
the ink and shifts those triples**, so the mask no longer matches the line it
was built to measure. It reports `width median 2.0` off 18 stray pixels and
exits 0.

The instrument's `selftest` PASSES (2 px -> 2.0, 5 px -> 5.7, dilation
detected, empty frame refused), so this is not a broken tool — it is a tool
whose precondition the shell quietly removed. **This is the fourth documented
case on this creature of a gate going silent rather than failing**, and by the
project's own rule silence from it is not evidence. Filed for the next pass;
not a publish blocker, because the picture answers the question the gate can no
longer reach.

The same limit makes the pass-11 side unmeasurable by that tool for a second,
independent reason: `webm2rgb.py`'s own docstring says VP9 CRF16 is LOSSY and
recovered frames are the SHIPPED pixels, never byte-identical to a render. It
correctly reports "NO INK PIXELS -> vacuous" rather than inventing a number.

### What the pictures actually show (all at native 384x240 unless noted)
* **THE BODY IS A BALL.** Unmistakable beside pass 11's egg/teardrop. The
  single clearest win of the pass.
* **THE SHELL CROSSES THE INK AND FADES INWARD, INK READABLE.** Confirmed at 8x
  on my own render, and it reads on BOTH backdrops — the orange sky of `hover`
  and `rest`, and the violet/pale bloom of `channel` and `crackle`.
* **THE MIST IS A TRAIL, NOT A FIELD.** `rest` (standing) pass 11 carries a
  broad cyan cloud plus grey blocky haze; pass 12 has **almost none** — a few
  specks on the antenna. `hasty` (travelling) still streams a trail. The speed
  gate is visibly doing real work, and the standing/travelling difference is
  now a real difference rather than a rounding error.
* **THE NODULES MOVE INDEPENDENTLY AND IT IS VISIBLE.** The committed
  `nodule-solo` four-tile plate gives four genuinely different loop
  silhouettes — A alone, B alone, C alone, and mid-down-outers-up. Not a phase
  offset of one curve; four different shapes.
* **THE ANTENNA IS THE OTHER BIG WIN, and I nearly missed it.** At native the
  band looked washed out; at 5x that turned out to be the pale BACKDROP behind
  it. Pass 11's antenna is a smooth crimson hose with no outline and no
  surface. Pass 12's is a bright pink band with a solid black ink line, a
  visibly ROUND nodule carrying its own highlight, and coarse grain running
  along it. It reads as a drawn limb with knuckles.
* **`startle` f26**: the loop opens wider and reads as a loop with thickness
  rather than a thin arc.
* Nothing new clips into anything in the frames examined; the ground-contact
  question is answered with the committed 3D probe below, never from pixels.

### NOT a blocker, but the owner should know it is an OPEN QUESTION
On `channel` the mana cluster sits against the violet planet bloom, and the
clip designated the house mana look is also the clip whose backdrop most
competes with it. Lane B raised this as an open question rather than deciding
it. I am shipping it as-is and flagging it rather than tuning it on my own
judgement mid-publish.

## ⚠ THE HEADLINE FINDING — the headstand sinks 87 mm through the floor

The brief named five things to check by eye, and one of them was **"nothing new
clips"**. Something new clips.

Found with the COMMITTED `manafold-probe.exe` (built to its own output tree so
it could not disturb the running render), against **posed vertices**, never
from the rendered frame — CLAUDE.md is explicit that the pixel shortcut is
unsound and once reported a barely-touching clip as 94.8% submerged.

At HEAD (`d820b574`), **`slot 13` = `manafold-trick`, the headstand**:

    slot 13 (200 keys): min clearance 19 mm at key 75 sub 1 — FAIL
    slot 13 DECLARED CONTACT keys 78..156: deepest vertex -87 mm
        (declared -25, accepted -60..-5) — FAIL
    CLEARANCE VIOLATED (< 40 mm)          [probe exit code 1]

**Every other slot in the bank passes.** Slots 0–12 and 14–16 are all OK.

### Proving it is THIS pass, not an inherited fault
Rather than argue from plausibility, I built the same probe from the commit
immediately BEFORE the pass-12 body work — `8996c51b`, the last commit that is
only a probe change — in a separate `git worktree`, and ran it:

|                        | BEFORE (`8996c51b`) | AFTER (`d820b574`) |
|---|---|---|
| slot 13 min clearance  | **73 mm** OK        | **19 mm** FAIL (floor 40) |
| deepest vertex         | **−34 mm** OK (band −60..−5) | **−87 mm** FAIL |
| probe exit code        | **0** — "CLEARANCE CONTRACT HOLDS" | **1** — "CLEARANCE VIOLATED" |

73 → 19 and −34 → −87. **Caused here**, by A1/A2/A3: the body is rounder, the
breath is 39% deeper and the pitch more than doubled, and the plant depth
constant under it did not move with them.

### The part that belongs on the record
**Neither implementer's findings mention it.** The probe is committed, it is the
project's own designated instrument for exactly this question, and it **exits
non-zero** — so this is not a gate that failed to exist, nor a gate that was
blind. It is a gate that **failed to be consulted**. That is a fourth, distinct
failure mode alongside the three this pass already recorded (a probe blind to
vertex effects; a probe passing a visibly-broken stub at 1087 against 1120; a
gain ladder that came back flat).

### What I did about it — and what I did NOT do
I did **not** fix it. Changing `kTrickPlantRootMm` is authoring, it belongs to
the implementer lane, and it would require a re-render; doing it inside a
publish pass is how a publish quietly becomes an unreviewed code change.

I did **not** quietly pull the tab either. A tab that disappears is silent
omission, and "partial delivery is fine and said out loud; silent omission is
the only forbidden outcome" is this pass's own reporting rule.

**I rewrote the caption I had already written.** An hour earlier I had written
that the contact *"was re-checked rather than assumed to have survived"* — a
sentence that would have become the FOURTH quietly-false caption on this page,
and the second one false the day it was written. It now carries the measured
regression, both probe results and the before/after, and the card blurb carries
a card-level warning so it is visible without opening the tab.

Evidence committed **beside the creature**, not in this run folder, which the
next pass would orphan:
`Upheaval/creature/Manafold/pass12-publish-evidence/` — both probe logs, the
nodule gate and its failable leg, the 22 CRCs, the by-eye plates.

## The nodule gate, re-run on MY clean build (not inherited)
`manafold-nodule.exe`, built from this lane's clone of `origin/main`:

    identity: local pose == committed slot 16, 2304 quats
      0  A alone      198  273  253   ok
      1  B alone        1  212  385   ok
      2  C alone        1    4  204   ok
      3  mid down     264  360  369   ok
      frame 156: B -86 (down)  C +55 (up)  separation 141 mm   ok
      VERTICAL REACH ball A  -7 .. +3 mm  (200 mm requested — DECLARED GAP)
    PASS: 0 failure(s)

Reproduces the findings' numbers exactly, on a binary I built. Both failable
legs engage: `--fail-mute B` gives 1 failure, and **`--fail-ignore` — which is
pass 11's own behaviour — gives 7**, all three nodules dead. The gate genuinely
separates this rig from the previous one.

## Render complete
**RENDER_RC=0**, 22/22 subjects, ONE binary invocation, binary md5
`925f58d8d37f7dc1118adaacebeff115`. All 22 sequence CRCs recorded to
`render-p12-crcs.txt` and committed. `manafold-hover` and `manafold-inspect`
are both **0xA7A197B1** and `diff -rq` finds all 600 `.rgb` frames identical
(only `meta.txt` differs, which carries the subject name) — so the Inspect
tab's byte-identity claim is TRUE for pass 12 and is not an inherited pass-7
number.
