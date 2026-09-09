# Task Log: RUN-20260908-0957 - [Describe objective here]

**Created:** 2026-09-08 09:57 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0957-manafold-p12-final-publish/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 09:57 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0957
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

## 09:40 — the fix pass landed; this run RENDERS AND PUBLISHES it

Lane: `manafold-p12-fix/` (reused as the publish lane rather than cloning a
sixth — it was already at the tip and clean, and the disk has been at zero once
this week).

Starting SHAs, both at `origin/main` with nothing unpushed:

    zhaozhou   0ecf69b6
    Upheaval   e3bae84

**What this bank contains that the live page does not.** The fix pass closed
five of the reviewer's items, and four of them were the same fault: the knob
every previous pass had turned was **not connected to the thing being judged**
(gotcha §18, four times in one pass).

* the corpse stops standing up — `hold_last`, never set on any Manafold clip,
  while Zixxtrixx has had it since run 0326
* one mana light RATIO for the bank — each lamp had been rounding independently
  and `hit` collapsed all four into one light
* the free-floating orbs — `kWanderCount` authored "of kMoteCount", subtracted
  as an absolute, so the garnish cut **promoted** the drifters 15% → 54%
* `inspect` no longer duplicates `hover` — 0 of 600 frames identical
* **the eye travel driver, recovered.** It was stranded on the branch of an
  agent a rate limit killed; only the channel itself had reached main, so five
  owner paragraphs (D9 §6, §6.1, §6.2, §12.2, §12.3) were undelivered.

### 09:41 — build: `--clean`, and the reason is now gotcha 19

    tools/reel/build-direct.sh --output out/p12final --clean cel
    md5 5ed169464ea58ff67e64ec6814c7ece7   (recorded BEFORE the render)

`--clean` is not caution. `build-direct.sh` compares timestamps on the `.cpp`
only and **has no header dependency tracking**, and every constant in this pass
lives in a header. Written up as `09-ENGINE-GOTCHAS.md` §19 before the render
started, because it is the stale-binary trap reached by the road CLAUDE.md
currently recommends as the *safe* one.

### 09:43 — render: 28 subjects, ONE binary invocation

22 last time, 28 now: `taunt3`, `lasso`, `blown`, `flight` and the two deaths
have all been added since. The list is taken from `creatures.json`'s live
declarations, not from memory — twice this week finished work was one file away
from being invisible, and both times the file was that one.

### NEXT STEPS — written down BEFORE the render lands
1. encode, **with no `-SkipMediaCheck`**
2. LOOK at the eye travel and the two deaths specifically
3. assemble, deploy `-Branch main`, verify from production
4. then items 5, 6, 7, 9, 10 and C3, which the fix pass named rather than omitted

⚠ **`-SkipMediaCheck` is not available to this run.** Using it twice last night
was judged UNSOUND by QA and it was: the flag is overloaded and also disables
`checkfresh.py`, so **six live mana clips shipped from the previous generation**
— the exact fault `checkfresh.py` was written to catch, defeated by the flag
that was supposed to only skip decodability.

### 09:50 — lane sweep, while the render runs
Audit clean: **no unpushed commits anywhere**, 16 lanes scanned. The p12-publish
run log was committed and rebased onto main first (45 lines that existed in one
place). Deleted `manafold-p11-L`, `-p12-pub`, `-p12-qa`, `-p12-review`, `-p12-w3`.
The review lane's 8 orphaned commits were preserved to
`origin/archive/p12-review-runlog` before it went.

### 10:05 — LOOKED at the first bank, and it changed the pass

The two deaths: **the corpse holds.** Twelve consecutive frames at the tail of
`death-drop` are the same slumped pose, and frame 0 beside them is plainly a
different, upright, brighter animal. The antenna is folded over, the mana is out.
That is the highest-damage item on the reviewer's list and it is closed.

`curious`: **the eye travel reads, and the stars survive it.** The eyes sweep
across the ball, the lozenges tilt as they go — §12.3's own-axis rotation — and
a yellow star sits in each one at every extreme I sampled. Five owner paragraphs
that were driven by nothing yesterday are on screen.

⚠ **And then `channel` — the clip he named the house mana look — was the worst
frame in the bank.** A near-white mass filling the left half, the creature a dark
shape in front of it, the mana barely legible.

### 10:10 — Q-B1 ANSWERED: the backdrop bloom is dropped

The fix pass left this as an owner question and was right to build the plate
rather than decide from taste. **But the owner has already ruled twice on
effects at exactly this scale** — D8 §1, the mist that *"covers the screen"* when
he wanted *"a tiny smidgen"*; D8 §4, the shell *"thickened too much"*. So this is
his rule applied, not my preference substituted for his answer.

Measured before deciding, on the comparison side where measurement belongs:

    manafold-channel   18.9% of the frame near-white
    manafold-crackle   18.2%
    every other clip   under 1%

**The two clips carrying the bloom are the two whose entire job is showing the
mana.** Dropping it cannot hurt anything else, because it is nowhere else.

**And it unblocks an owner ask open since 2026-09-06** — D8 §2, *"the particle
effects from the lightning should've made shapes… right now: no shapes, can't
see them"*. He named both candidate causes himself, occlusion or regression.
**It was occlusion.** Three passes tuned three families of lightning constant
against a backdrop problem — §18's signature, and the false comment claiming the
bloom *"sits OFF to the side"* is what kept pointing them at the lightning. Both
the constant and that comment are now fixed.

Re-render, because a one-clip re-render is how six mana clips shipped from the
previous generation last night. `kU02BackdropBloom` flips it back in one line and
`ZHAO_U02_PLANET=1` shows the old mood with no rebuild.

    binary md5 5ed16946… -> a8a54f24…   (the change reached the exe -- gotcha 19)

**Verified after: the white is gone, and the dotted lightning arcs over the
crown are legible for the first time.**

### What I looked at and did NOT fix — all named, none silent
* **`taunt3` is not funny** — twelve sampled frames are one standing pose with a
  wiggle. No beat, no anticipation, no lean. The reviewer's item 6 stands exactly
  as written.
* **The star is not centred in the lozenge** — visible in `curious` and `channel`.
  Item 9, and `kEyeShiftPivotMm` is half-written, so it is a skeleton fix first.
* **`flight` has a grey haze that ramps over its last frames** — 44 → 87 → 227 →
  441 grey pixels over frames 340→351, then snaps to 97 at the loop. It grows
  smoothly, so it is likely authored and only the WRAP is wrong. New clip, never
  seen by anyone; recorded rather than chased.

### 11:25 — ⚠ THE BLOOM FIX WAS HALF-LANDED, AND MY FRAME READER WAS LYING

Two corrections, both mine, both caught within an hour of each other.

**1. The bloom switch only reached `channel`.** I put `kU02BackdropBloom`
inside `u02_common()`, which is the slot-2 path. **`crackle` sets
`s.planet = 1` on its own subject**, outside that path — so channel lost the
bloom, crackle kept it, and the card text I had already written claimed both.
09-ENGINE-GOTCHAS §16 exactly: **a partial fix is more dangerous than none,
because the half that worked is the half you look at.** I found it in the
rendered bank; the pass-13 architect found it in the source independently.
Constant and a `u02_planet_on()` helper now live at namespace scope so one
switch governs every clip. Verified: crackle near-white **18.2% → 0.5%**.

**Killed the encode at 13 of 28 rather than ship a mixed bank.** Re-rendering
all 28 from `ffae071e`, binary md5 `6d6665c2…`.

**2. Every colour word I wrote this morning was wrong.** `.rgb` reel frames
carry an **8-byte header**; I read them with a bare `PIL.frombytes`, which
rotates every pixel's channels. So "blue creature", "green sky", "gold mana" —
all false. **`tools/reel/rgbframe.py` exists precisely to prevent this** and its
docstring says *"USE THIS ONE. Do not write another frame reader."*

**And I put the broken recipe in the pass-13 architect's brief**, which cost it
half a morning believing the bank had changed colour. Written up as
`10-GATE-CHECKLIST.md` **item 38 — a brief is an instrument, and it can lie like
one**: it is read first, by an agent with no context to contradict it, which
makes it the most trusted instrument here and the only one nobody checks.

⚠ **What survived, stated deliberately.** Near-white area and grey-pixel counts
are **invariant under a channel rotation**, and so is every judgement about
pose, motion and shape. So the corpse holding, the eye travel reading, the
bloom covering 18.9%/18.2% against <1% elsewhere — all stand. **Only the colour
words were void.** Discarding the sound conclusions along with the broken ones
is its own kind of damage.

### 11:40 — PASS 13 OPENED, two implementers running

The architect's `PASS-13-PLAN.md` is committed. Its own §0 is the instrument
warning above, made binding for every pass-13 lane. Three findings I did not
have:

* **Every travelling clip teleports at its wrap**, not just `flight` — grey
  splash `drift` 295→**729**, `hasty` 95→238, `fall` 93→126, all snapping to
  ~zero at frame 0. **Same mechanism as the corpse standing up**, applied to a
  wrapping root. One opt-in flag cures four clips.
* **The main idle is faceless for ~2.5 s** — the travel channel runs two
  always-on sines, so the eyes never dwell.
* **Item 5 downgraded by looking**: the crown visibly arches and reconfigures
  in `hover` f180–250 now that the motes are cut. No dedicated work item.

Lanes: **IMPL-A "face"** (R1) in `manafold-p13-a`, **IMPL-C "engine+reel"**
(R2/R6/R7/R8) in `manafold-p13-c`, disjoint files. **IMPL-B "performance"** is
held: C must land the `Clip` wrap flag before B flips it.

### NEXT STEPS — written down BEFORE the render lands
1. render finishes → **verify no renderer process is alive** (28 directories
   existing is NOT 28 clips rendered; that check fired early once today and
   `mana-stack` had 193 of 601 frames)
2. encode, **no `-SkipMediaCheck`**, then `checkfresh` + `checkmedia`
3. assemble, deploy `-Branch main`, verify from production
4. release IMPL-B when C reports the flag landed

### 20:15 — THE SECOND BANK WAS THROWN AWAY TOO, and for a better reason

Killed the encode at 8 of 28. Not a fault this time: **a better answer arrived
while it was encoding.**

IMPL-C had taken my R6-bis ask — separate the bloom's magnitude from the sky it
sits in — and landed `kU02NightSunMagPx = 25`, authored **by eye at 4× on the
frame it found by sampling for the most cyan**. Its three-state plate is not
close: the shipped bloom swallows the left half; the planet-off day sky reads
pink-on-pink and the mana goes soft; **the night keeps the violet, drops the
white to a small low moon, and the cyan crackle reads better than on either.**
crackle near-white **15.16% → 0.57% with the sky intact.**

Shipping the day sky would have been shipping a mood regression I had already
been shown a better answer to. Rebuilt from `38cd8861` (md5 `c5a71542`), which
also carries IMPL-A's face work, and re-rendered all 28.

**Three full banks were rendered today and two were destroyed.** The rule that
made both calls is the same: **27 clips from one generation beside 1 from
another is the fault this site has a gate to prevent**, and it has already cost
a published page once. A partial re-render is never the cheap option.

### 21:20 — ⚠ THE CARD ALMOST CARRIED ITS SECOND FALSE CLAIM OF THE DAY

I had already written the paragraph saying the travelling clips stop teleporting,
quoting IMPL-C's real measurements — `flight` 7.69× → 0.88× of a typical frame's
motion, and so on. **Those numbers are true of a build that is not this one.**

`wrap_root_delta` defaults OFF so Zixxtrixx stays bit-identical, and **the lane
that flips the four opt-in lines had not landed when this bank rendered.**
`grep -c "wrap_root_delta = true" manafold_clips.h` → **0**.

Caught by looking at the frames instead of quoting the headline:

    flight grey pixels  f340 44 -> f351 438 -> f0 62      (pre-fix: 44 -> 441)

Within noise of the fault. **10-GATE-CHECKLIST item 12 exactly — reproduce the
headline, do not inherit it** — and the second time today the card would have
claimed a fix that reached only part of what it named.

**What I DID verify as true of this bank**, on the shipped frames and constants
rather than the reports:

    kEyeBulgeMm 88 -> 40, kEyeDeepMm 90 -> 40     A's flattening landed
    kU02NightSunMagPx = 25                        C's night landed
    channel near-white 18.9% -> 1.06%             the white is gone
    channel sky (16,12,49)                        the violet night is BACK

### 21:30 — OWNER RULED THE EYE PEAK: 39°

Asked to keep 45° or pull the biggest glance to ~30°, he split it: ***"Go to 39
degrees."*** Closer to his original than to the safe answer — the big look stays
big, it just comes off the limb.

`kEyeGlanceOutPm[0]` 1000 → 867 (867‰ of 45° = **39.015°**). Relayed to IMPL-B
with two constraints: **do not touch `kEyeTravelMaxDeg`** (it is the clamp, and
lowering it would drag the other two glances off the angles authored by eye),
and **do not treat 39.015° as delivered until it has been looked at** — he chose
a number from a description, and the number only carries the intent.

⚠ **This bank ships at 45°**, because it was 26 of 28 subjects into its render
when he answered. Named on the card so the two pages are not read as a
regression.

### NEXT STEPS — written down BEFORE the encode lands
1. encode finishes → `checkfresh` + the ~22 min decode sweep, **neither skipped**
   (`-SkipMediaCheck` is now a hard error; IMPL-C split it into two switches)
2. assemble, deploy `-Branch main`, verify from production
3. IMPL-B lands → next bank carries taunt3, blown, the gutter root and the 39°
4. **then reviewer + QA on pass 13** — the owner's mandated sequence, and pass 13
   has had neither

### 22:00 — THE THIRD BANK DISCARDED, and this one is the convergence point

IMPL-B landed: `taunt3` re-timed with a real attack curve, `blown` retimed,
`death-gutter`'s root step **240.0 → 102.6 mm**, the four wrap opt-ins, and the
owner's **39°**. Killed the encode at 10 of 28 and rebuilt from `25d86c03`
(md5 `c5a71542`).

**Three banks rendered today, two destroyed, and this is the last** — not because
I ran out of patience but because **all three implementer lanes are finished**.
Nothing more is landing, so this is the first moment the bank stops being a
moving target.

⚠ **The expected-frame-count check earned itself.** `blown` came back 293/393 and
looked like a truncated render. It is not: IMPL-B retimed the clip from 196 keys
to 146, so **293 is the correct new length** (`meta.txt` says `frames=292`).
Verified against the clip's own metadata before touching anything. A check that
flags an intended change is working — the failure would have been believing
either "truncated" or "fine" without looking.

### 23:00 — ⚠ THE WRAP FIX IS ON, MEASURED, AND STILL NOT THE WHOLE FAULT

Both lanes report the loop seam fixed and both are honest: the wrap frame's ROOT
motion falls to **0.90×** a typical frame's on `flight`, 0.46× on `fall`. I
verified the flags are genuinely switched on — four `wrap_root_delta = true`, in
`build_drift`, `build_hasty`, `build_fall`, `build_flight` — and that this bank
was built after them.

**The grey ghost is untouched.**

    flight  f340 44   f350 151   f351 477   f0 62      (pre-fix: 44 -> 441)
    drift   f290 188  f299 427   f0 47

And by eye `flight` still jumps between its last two frames. **So the root
displacement was never what the ghost was made of.** Two real measurements, both
true, neither of them the picture — which is the same lesson as every other one
today, arriving through a *correct* fix rather than a wrong one.

Sent to QA to adjudicate, with four questions including **"is my grey metric
junk?"** — it was written fast and may be counting the mana trail.

**The card's wrap paragraph has now been wrong in BOTH directions in one day**:
claiming the fix this morning, then "built but not switched on" this afternoon
(true when written), now corrected to what the frames show. It stays on the page
rather than being dropped, because a page listing only the fixes that worked is
not evidence of anything.

### 23:10 — gotcha 20a: a job can OUTLIVE its killed tool call too

A `cp -r` ran for eight minutes after its tool call was torn down. The tell was
`rm -rf` failing with *"directory not empty"* on a directory just emptied, and a
file count going **286 → 321** a minute apart. Killed by PID. IMPL-C
independently found a render holding a core an hour after its shell died.

**A job's lifetime and its tool call's lifetime are unrelated in both
directions**, and one repeated count sixty seconds apart says which.

### NEXT STEPS — before reading anything else
1. encode → `checkfresh` + the full decode sweep, **neither skipped**
2. commit media, assemble, deploy `-Branch main`, verify from production
3. **launch the REVIEWER** against the published webm (the true artefact) once
   the media is committed — QA is already running
4. QA's verdict on the grey ghost decides the card's wrap paragraph

### 02:14 — PUBLISHED, and the first attempt had already succeeded

`deploy.ps1 -Project upheaval -Branch main`. **All gates green**: noindex,
`checkfresh` **28 of 28 fresh / 0 stale**, and the full decode sweep of **641**
declared media files.

⚠ **The first run failed AFTER passing every gate**, on
`Test-Path : Illegales Zeichen im Pfad` for `C:\...\publicenders`. The `\r` in
`"public\renders"` was an actual **carriage-return byte**, because the `.ps1` had
been written from a Python script where `\r` is CHR(13). Two occurrences,
**invisible in the source, in `git diff` and in review** — found with `cat -A`,
which shows them as `^M`. Written up as `09-ENGINE-GOTCHAS` §21.

**And it had already uploaded.** The failure was in the deploy-RECORD code, which
runs after wrangler; the second run reported *"1333 already uploaded"*. So the
gates and the upload were fine and only the bookkeeping broke — which is exactly
why no gate could have caught it.

**Verified from production, not from the deploy's own word:**

    taunt3   local 2,423,467 bytes == live 2,423,467   MATCH
    flight   local 1,472,044 bytes == live 1,472,044   MATCH
    the card's newest text present, exactly one noindex

### 02:30 — THE REVIEW LANDED AND IT RE-FRAMES THREE PASSES

**"The model is good. The bank is not yet."**

⚠ **THE EYE IS BUILT CORRECTLY AND PROPORTIONED WRONGLY.** Sheet-to-render,
front to front, ball width normalised:

    lens aspect   ours 4.0-4.9      sheets 2.6-3.2
    star / lens   ours 0.12-0.23    sheets 0.52-0.60
    and both eyes sit entirely in the ball's LOWER HALF

So three passes have been re-centring a star about **a third of its drawn size**
inside a lens **half again too long**. Registration, parallax and depth were each
correctly diagnosed and correctly fixed — **and all three were answers to a
question the eye does not raise.** The owner asked twice for the eyes to be
centred and was told twice that they had been.

**This is the fifth time on this creature that a careful, correct fix was aimed
at something other than the fault**, and the first time the miss was in the
*subject* of the measurement rather than its method.

Other findings, in the review's order: **`taunt3` has no HOLD anywhere** across
368 frames, so there is no beat and no punchline (the attack curve was right and
insufficient; the funniest thing in the bank is `trick`'s handstand, which nobody
authored as a joke); **the body goes POLYGONAL when it squashes**, which sits
across *round* and *bouncier* both; `hasty` loses the creature for ~40 frames;
`blown`'s apex has nothing happening ON it, so rotation not light; the star still
vanishes past 30°.

**The review also killed three claims before they cost a pass** — including a
helper's "the mana composites at quarter resolution", refuted at 0.0% of aligned
blocks constant (it was VP9 quantisation in the delivered webm) — and **two of
its own numbers.**

### 02:40 — pass 14 opened; five lanes swept
Fable architect briefed on the review, and told explicitly to **re-derive the eye
measurement itself** and to **adjudicate QA against the review** rather than
inherit either. Items 6 and 7 have survived four passes each: no fifth round of
constants.

Lane audit clean — **no unpushed commits anywhere**, 14 lanes. QA's two worktrees
removed properly first, then `manafold-p13-{a,b,c,qa,review}` deleted.

### 04:00 — PASS 14 EXECUTING. Wave 0 (mine) is done.

The architect's plan solved **two four-pass mysteries by finding missing degrees
of freedom rather than mistuned knobs**:

* **The eye**: the star's plate normal is fixed body-space **+X** while the eye
  sits at **28.3° azimuth**, and **no orientation DOF exists — that bone is
  translation-only.** Four passes of thickening were fighting a rotation the rig
  cannot express.
* **The lightning**: the look the owner approved, `edge-strands`, drew the figure
  **edges** with the strand. **Shipping took the mote half and not the edge
  half.** Four passes of particle constants never had a chance.

It also **corrected me and the review**: it re-derived the eye measurement itself
and found **~half the "eyes sit too low" reading is the 15° camera pitch**, which
nobody had separated out, and that **convergence is as much of the fault as
height** (the sheet's lens tops nearly touch; ours sit ~0.6 R apart).

Lanes: **FACE** (opens with the build-at-32 ablation — two streams blocked on its
answer), **REEL** (opens on the lightning mechanism, never a gain tweak), **PERF**
(taunt3 and blown's tumble; R2(b,c) blocked until FACE reports).

**Wave 0, all done:** R10.1 and R10.2 were already closed tonight (the wrap card,
the `blown` comment, FINDINGS-A's stale 45°). Owner packet built with **Q1's
picture** — and the 4× crop shows something nobody had asked about: **the mana
goes out when it dies but THE EYES STAY LIT.** Flagged, not answered.

### 04:20 — I RAISED A FAULT AND THEN REFUTED IT MYSELF

Of the two clips I had marked SUSPECTED:

* **`drift` — ESTABLISHED.** Clipped by the **left** frame edge at f283, back by
  f287. A brief edge clip, milder and different from `hasty`'s exit.
* **`fall` — REFUTED. I was wrong.** At f339 the creature **is** in frame, sitting
  **80 px higher** than I was looking, with its trail below it. I raised it off a
  native-scale contact sheet and it survived one round of my own review until a
  2× crop killed it.

**Four instruments on one small question tonight** — *"is the creature in
frame?"* — three pixel masks outright wrong, the contact sheet wrong once and
right once, and **a 2× crop of the named frame right every time.** Written up as
checklist **40** (calibrate a presence metric on a known-negative) and **41** (a
contact sheet finds a candidate; it cannot confirm one — and *absence* is the
claim to distrust, because it is the easiest thing to see when you are looking
for it).

**The cheapest reliable instrument was the last one I tried**, which is the art
law arriving from the measurement side.
