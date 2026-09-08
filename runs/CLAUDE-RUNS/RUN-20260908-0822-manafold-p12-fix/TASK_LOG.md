# Task Log: RUN-20260908-0822 - [Describe objective here]

**Created:** 2026-09-08 08:22 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0822-manafold-p12-fix/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 08:22 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0822
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

## 2026-09-08 — lane setup and item 1 root cause

**Lane:** `C:\programmieren\zencrifice\manafold-p12-fix\{zhaozhou,Upheaval}`.
Cloned from the local checkouts, then `git remote set-url origin` to the GitHub
URLs and **verified both** (`untitled-game.git`, `zhaozhou.git`). The reviewer's
warning about `origin` silently resolving to a local path is handled: the lane
remotes are separate named remotes (`ln*`, `zn*`), never `origin`.

**Base chosen.** Both repos have five divergent lanes. Measured with
`git rev-list --left-right --count`:
- Upheaval: `manafold-p12-qa/main` (845e271) is 12 ahead of the coordinator
  branch and **0 behind** — a strict superset of coord, review, pub and
  GitHub `main` (e11de6c). Base = that.
- zhaozhou: `manafold-p12-qa/main` (178973a2) is 6 ahead of coord, 1 ahead of
  w3, **0 behind** either. Base = that.
- NOTE: the review's "133 commits unpushed, one machine" risk is **already
  resolved** — GitHub `main` (e11de6c, 2026-09-08) contains `creature/Manafold`.
- NOTE: this base **includes Wave 3** (eye travel, nodule vertical travel,
  flight bob), which was NOT in the build the reviewer judged. Renders will
  differ from the reviewed ones for reasons beyond my fixes. Declared, not hidden.

### Item 1 — THE CORPSE STANDS BACK UP: root cause found, and it is not a knob

The authored keys are **already correct**. `build_death_drop` / `build_death_gutter`
write the dead pose, `kDeathRestRootMm` and a bit-zero deform at every key from
`B.settle` to `K-1`. Nothing in the tail is alive.

The resurrection is in the **presentation interpolation**, not the authoring.
`reference/src/zcreature/creature_core.cpp` computes the sub-frame partner as

    frame + 1 >= clip.frame_count ? (clip.hold_last ? frame : 0) : frame + 1

at four sites (pose quats, root displacement, `deformation_sample`,
`deformation_frame`), plus the `wrap` lambda at line ~230 used to bake midpoints.
With `hold_last == false` the final key's sub-frame **blends toward key 0 — the
alive hover pose**. That is the 11 px jump and the ~6,500 changed pixels, and it
is two frames because a key is shown for two sim ticks.

`grep -rn hold_last` over the tree: **Zixxtrixx sets it on eight clips**, including
`zixxtrixx.h:4550` — *"one-shot: the corpse holds; no wrap-to-stance flash"*.
**`manafold_clips.h` sets it nowhere.** The sibling creature already solved this
and the fix did not travel.

**Why the previous pass could not tune its way out (09-ENGINE-GOTCHAS §18).**
`kDeathOpenKeys` (manafold_art.h:1930) exists *because they hit this exact seam* —
its comment says "at the clip's LAST key that stream interpolates toward key 0 …
no matter how carefully the tail is zeroed". They fixed it **for the deform only**,
by easing the deform in from zero at key 0. Root and quats were left wrapping.
The knob being turned (tail length, open keys, the caption) was never the thing;
`hold_last` is.

**Fix:** `c.hold_last = true` on both death builders. One line each.

### Item 1 — DONE. Gate Q4 added, leg witnessed failing (509.1 / 411.9 mm).
Committed b5717584, pushed, landing verified with `git branch -r --contains`.

### Item 2 — the mana lighting: the ratio, not the range

**Mechanism.** `u02_ml_turns` rounded EACH SOURCE INDEPENDENTLY:
`round(frames*base/420)`, floored at 1. The four lamps are authored 1:2:3:4.
Independent rounding destroys that ratio on any clip whose length is not near
a multiple of 420:

    channel 420f  1:2:3:4   the model
    blown   392f  1:2:3:4   the model
    hover   600f  1:3:4:6
    curious 180f  1:1:1:2   three lamps in LOCKSTEP
    hit     140f  1:1:1:1   all four collapsed into ONE light

Four lamps at 1:2:3:4 trace one closed Lissajous path; a clip running whole
turns walks ALL of it, so its brightness range is the path's range. A different
ratio is a DIFFERENT closed path with a different range. That is D9 §4's "all
videos have different mana lighting configurations" as an arithmetic fact.

**Fix.** One cycle count per clip, every source takes its authored multiple:
`u02_ml_turns(base, frames) = u02_ml_cycles(frames) * base`. Whole turns
preserved, so no loop pops. `channel` (420f) and the 800f lab are
byte-identical — the house look is again what the bank moves ONTO.

**MEASURED, on an EXACT differential mask** (`ZIXX_HIDE_CREATURE`, bodymeter's
method — every pixel the creature hook changed; no threshold, cannot take the
terrain). Mask painted green and LOOKED AT: clean creature, no ridge, no sky
(`mask-green-check.png`). Body Rec.601 luma, mean over the mask, per frame:

    clip       BEFORE lo..hi (swing)     AFTER lo..hi (swing)
    channel     84.2..110.5  (26.3)      84.2..110.5  (26.3)   byte-identical
    curious     90.5..115.6  (25.1)      87.7..114.0  (26.3)
    hit         91.8..114.5  (22.7)      87.2..113.2  (25.9)
    hover       76.6..111.8  (35.2)      72.6..111.8  (39.2)   <- camera ORBITS

    swing spread, static-camera clips:  3.6  ->  0.4   (9x tighter)

`hover` is the one clip of the four with `orbit = true`; its extra swing is the
CAMERA, not the light, and it is authored.

⚠ **THE REVIEWER'S NUMBERS DO NOT REPRODUCE, and this matters.** REVIEW §2
reports `channel` 65–78 and `curious` 105–137 — "channel never gets brighter
than 78; curious never gets darker than 105", i.e. non-overlapping. On an exact
mask the shipped build gives channel 84.2–110.5 and curious 90.5–115.6, which
**overlap over almost their whole extent**. The reviewer flagged that table as
"indicative of ordering only" and confessed their saturation mask had already
taken the sunlit ridge twice. `channel` is the one clip with a huge pale planet
bloom filling half the frame, so a saturation rule selects very different pixels
there than on `curious`. **The "two different-coloured creatures" figure is most
likely a mask artefact.** The ratio collapse underneath it is real and is fixed
on its own merits.

⚠ **THE LIMIT THAT REMAINS, and QA is right about it.** Warm's period is still
frames/n, and n = 1 for every clip under 630 frames, so hover-vs-hit is still
4.3x in RATE. This is arithmetically forced, not an oversight: period = frames/n
with n a positive integer, and a 140-frame clip cannot run a 420-frame period
AND close its loop. **Rate equality, ratio equality and seamless looping are a
trilemma; any two are available.** Shipped pass 12 chose rate+loop and lost the
ratio. This chooses ratio+loop and keeps the rate spread. I chose the ratio
because the owner's words name CONFIGURATION and the reviewer measured RANGE,
both of which are the ratio. **Flagged for the coordinator, cheap to reverse:
one function.**

**Ablation committed** rather than improvised: `ZHAO_U02_ML=indep` restores the
shipped behaviour, so before and after come from ONE binary.

### NEW ITEM 1 (coordinator) - the eye travel driver, recovered

Driver found: fd393952 "WIP salvage: manafold-p12-2a/zhaozhou at the weekly
rate limit", 42 insertions, parent 7c38dfb2 - the same base my branch is off,
so it is a sibling and cherry-picks clean at zero offset.

THE GATE THAT SAID "0 OF 23" COULD NEVER HAVE SAID ANYTHING ELSE.
After applying the driver, Q2 still printed 0.00 deg for every clip. Not the
stale binary - a --clean rebuild said the same. Q2 read

    const double y = q.q[1] / 16384.0;   // "identity quat16 is (0,0,0,1<<14)"

and both halves are false. zref_creature.hpp:
quat16_identity() returns quat16{{kQuatOne, 0, 0, 0}} - w is lane 0, so the
lanes are (w,x,y,z) and lane 1 is X. The travel is applied with quat_y(),
which lives in lane 2. The gate read the X lane of a pure-Y rotation and was
structurally incapable of reporting travel.

QA's headline "0 of 23 clips drive the eye travel" was independently TRUE -
the grep showing apply_eye_travel called only by its own gate was correct -
but this instrument did not show it, and would have gone on printing 0.00
after the driver landed, sending the next pass hunting a channel that already
worked. Instrument #5 on this creature. Now read as an axis-agnostic
magnitude, 2*acos(|w|), so re-authoring onto another axis cannot blind it.

With the read fixed the driver works: 20 of 23 clips, bank max 45.00 of 45.
Slot 7 still, 15 the mana lab and 16 nodule-solo correctly have none.

Three bugs in the salvaged code, all found by looking at the numbers:

A - THE IDLE WAS PINNED AT THE STOP. Amplitudes 1000 + 300 against a +/-1000
    clamp. Both phase seeds are slot*k, so at slot 0 both are zero and the
    waves ran perfectly in phase: peak 1300 against a 1000 clamp is hard
    against 45 deg for ~44% of the loop. The gate showed it as a column of
    identical 45.00s - a servo on its limit, the opposite of "deliberate".
    700 + 300 now touches 45 only where the waves peak together. Readings are
    39-45 and varied.

B - "TWO INCOMMENSURATE PERIODS" WAS FALSE FOR SIX CLIPS. Both cycle counts
    were keys/divisor floored to 1, so under 122 keys they collapsed to the
    SAME frequency and peak travel was set by an accidental phase gap -
    pirouette and hit printed 31.55 deg, a number nobody authored. B is now at
    least one cycle faster than A by construction.

C - THE EYES SNAPPED DEAD. Both deaths guard if (!dead) antenna_knead, so at
    the settle key the carrier dropped from up to 45 deg to identity in ONE
    key, at the instant the corpse goes still. The nodules were protected from
    exactly this (their droop eases on fold_ease(gone)); the travel arrived
    with no equivalent. This is the half of the wave-2a edit that never got
    written. Now faded with 1000 - fold_ease(gone), reaching 0 exactly at the
    settle - continuous into the identity the dead branch already holds.

    GATE: Q2 now reports the worst PER-KEY STEP, the only thing that can see a
    switch-off (peak travel says nothing about it). Bound 8 deg/key.
    Faded: 1.27 / 4.00 deg. --fail-eyesnap leg: 13.35 deg at key 117 and
    39.55 deg at key 191 - exactly the two settle keys. Witnessed.

D - NO PER-CLIP GAIN EXISTED. kKneadClipPm and kNoduleClipPm both let a clip
    silence a layer; travel had none. antenna_knead now takes eye_pm = 1000 by
    default - no existing call site changes - and the deaths are its first user.

NOT VERIFIED BY EYE YET. These are rig numbers. The star-containment rules in
manafold_probe.cpp have only ever been measured on an untravelled bank, so
their numbers will move; treat any new failure there as real.
