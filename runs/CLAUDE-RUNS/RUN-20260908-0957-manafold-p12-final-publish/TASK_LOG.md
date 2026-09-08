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
