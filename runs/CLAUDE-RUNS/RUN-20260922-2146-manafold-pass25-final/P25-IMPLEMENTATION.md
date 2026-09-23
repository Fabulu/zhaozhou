# Manafold pass 25: implementation — THE FINAL PASS

**Date:** 2026-09-23
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-26-2026-09-22.md`
**Worker:** Claude (sole Opus implementer; no sub-agents, no Qwen, no HomeAI)
**Source:** Zhaozhou `manafold-pass25`, from the production-verified pass-24 main
`09108284`
**Verdict:** items 1 and 2 DONE and shipped. **Item 3 laddered and NOT
delivered, for a reason that is the pass's most useful finding** — the lever the
direction names does not calm the back ball, and the measurement that said it
would was answering a different question. Ships no byte change, declared.
**Not rendered here:** the 22-subject bank for publication, the encode, the
merge, the deploy — by instruction.

**Matrix: 240 / 240 PASS, 0 FAIL**, one invocation
(`P25-RECEIPTS/runmatrix_p25.sh` → `gate-matrix.txt`, log in
`gate-matrix-run.log`), run from a FROZEN copy of the script.

| family | legs |
|---|---:|
| normals (every gate binary, incl. mbolt's five legs) | 15 |
| controls fired and attributed (mbolt's four, mrod's four, the two exit-code positives, the three retired-name errors) | 13 |
| mrear mask controls, each with its exact declared mask | 13 |
| mspan controls | 37 |
| msmooth controls | 16 |
| protected legs | 27 |
| QA fault injections | 5 |
| selectors, RC 2 (the 91 carried forward plus 11 new) | 102 |
| identity + live history | 12 |

Three legs in the identity family are new this pass and each exists because the
alternative reads identically to a fault: `e-p24off` (item 1's per-SUBJECT
predecessor, reproduced through a named selector), `e-item3` (**no byte change**)
and `e-item3-live` (**the same knob moved, changing exactly the three subjects
that play slot 0**). The three `idfile` legs print only the basename of the file
they compare against, and two different files are called `crcs-ship.txt`:
`e-p24off` and `e-item3` compare against **pass 24's**, `e-ship` against **pass
25's**. The script says so at each line.

**The one-paragraph answer.** Avoidance is on all 22 live subjects and the bank
now has **zero** bolt/rod intersections against 54,595 before. The clearance is
raised from 46 mm to **96 mm**, chosen by eye — and the ladder found that the
knob is **not monotone**: 56, 66, 70, 76 and 106 mm all *introduce*
intersections, so the value had to be taken from the measured-clean rungs. The
eye ambience goes from 600 to **800**, the rung below the one where the star
reaches the lens rim, with the contrast against Curious and Startle checked
deliberately rather than assumed. And the rear-calm lever was laddered across
its whole range including fully off: carrier C's measured motion is flat to
1.3 %, the fixed-camera plates show the same motion at 1000 and at 0, and the
pass-24 "52×" figure turns out to be **changed pixels between two
configurations — an offset — quoted for a question about motion.**

---

## 1. ITEM 1 — THE ROLLOUT, AND A CLEARANCE CHOSEN FROM THE RUNGS THAT WORK

### 1a. One door, because the gate used to hold its own copy of the answer

Pass 24's reviewer found `manafold_boltgate.cpp` carrying its own per-subject
`avoid`/`split_n` columns and applying them **to itself** before measuring.
Nothing bound them to `zhao_reel.cpp`'s assignment. If a subject had lost
avoidance in the renderer while the gate's mirror still said `true`, mbolt would
have switched avoidance on for its own run, measured that, and printed
`B1 CLEAR manafold-hover: 0 of 38,152` — green, over a clip drawing bolts
straight through the rod. A checker whose two operands are configured by itself
cannot see the fault it exists for.

There is now one accessor, `u02::bolt_avoid_for(name)` / `bolt_split_n_for(name)`
in `manafold_art.h`, over one list `kBoltLiveSubjects[22]`:

* `subject_u02_clip()` sets `s.u02_bolt_avoid_rods` / `s.u02_bolt_split_n` from
  it. The three hand-set lines pass 24 left at the hover, inspect and crackle
  call sites are gone.
* `manafold_boltgate.cpp` reads the same accessor; its `avoid` and `split_n`
  columns are **deleted**. What the gate still owns is only what the renderer
  does not expose — the clip slot and the camera.
* `kU02LiveSiteSubjects` stays in `zhao_reel.cpp` because the committed
  live-history gate greps it out of that file and checks it against
  `creatures.json`; a `static_assert` binds it name-by-name and in order to
  `u02::kBoltLiveSubjects`, so the JSON check is true of the list the rollout
  uses.
* **It is keyed on the NAME, not on "is a Manafold clip."** `manafold-crackle-legacy`
  is the version-18 Wave D byte control and the diagnostics answer questions
  about the form; giving them this pass's lightning would have destroyed what
  they control for, and the destruction would have looked like a successful
  rollout.

### 1b. The rollout's own result

| | before (pass 24) | after |
|---|---:|---:|
| live subjects carrying avoidance | 3 of 22 | **22 of 22** |
| bolt segments intersecting the antenna, whole bank | **54,595** of 503,304 | **0** |
| subjects with any intersection | 20 | **0** |
| nearest bolt/rod pass anywhere in the bank | **4.4 mm** (blown f130) | **32.3 mm** (channel f247) |

The last row is the one the owner's complaint is about, and it is why the pass
is not just the rollout: **the tightest pass in the bank opens by 7.3x**, which
is what "a little more avoidant" had to mean in millimetres.

The two the census named as the worst untouched offenders — **death-drop
(16.6 %)** and **drift (15.3 %)** — are both zero. Every key and midpoint of
every clip. `P25-RECEIPTS/mbolt-census-clearance46.txt` and the shipping
`gate-matrix.txt` leg `n-mbolt`.

### 1c. THE NUMBER NOBODY HAD, and it is the owner's complaint

Pass 24's B1 read zero and the owner could still see the fault. That is not a
contradiction: **zero intersections is not "clear".** mbolt now reports the
CLOSEST APPROACH — the smallest gap between any drawn bolt segment and the
band, with the frame it happens on — because "it gets very close now and looks
like crossing" is a statement about proximity and nothing was measuring it.

At the pass-24 clearance the bank's nearest pass is **4.4 mm, on blown f130**.
At 384×240, with a stamped sprite chain a few pixels wide on one side and
contour ink on the other, 4.4 mm of model clearance closes back up into a
touch. That is the complaint, located, and it is what chose the ladder's frames:
blown f130, crackle f250 (12.1 mm) and hover f471, **sampled by badness from
mbolt's own per-frame csv rather than by index**.

It is reported and deliberately **not asserted against a threshold**. The
clearance is an art value; a gate that picked a minimum gap would be a
measurement on the generation side.

### 1d. THE KNOB IS NOT MONOTONE — measured over the whole bank

Raising the clearance does not monotonically reduce the intersections. It can
introduce them:

| mm | result | mm | result |
|---:|---|---:|---|
| **46** | clean, min 4.4 (blown f130) | 106 | **1 hit** (channel f247) |
| 56 | **3 hits** | **116** | clean, min 56.7 |
| 66 | **8 hits** | **130** | clean, min 36.1 |
| 70 | **11 hits**, worst −56.4 (blown f142) | **150** | clean, min 24.3 |
| 76 | **4 hits** | **170** | clean, min 43.6 |
| **86** | clean, min 45.8 | | |
| **96** | clean, min 32.3 | | |

`P25-RECEIPTS/clearance-sweep.txt`. The cause is structural, not noise:
`bolt_avoid_rods` is a FIXED-count relaxation (`kBoltAvoidSweeps`) with a
directed push, and a clearance big enough to shove a chord off one rod but not
past the neighbouring ball leaves it wedged where the remaining sweeps cannot
recover. Every dirty rung is `blown` or `channel`, on the tight rear pocket.

**So the value had to come from the clean list, and the ladder was re-rendered
on clean rungs only.** The table is written beside `kBoltRodClearanceMm` so the
next person to turn the knob knows to run the gate afterwards. It is open issue
1: not repaired here, because Direction 26 is art tuning of an *accepted*
mechanism and raising the sweep count would move every clip's bolt path on a
final pass.

### 1e. The ladder, and the visual reason for 96 mm

Three clips, at 8× and at native, on the frames above
(`P25-LOOKS/01`–`05`).

| rung | what was seen |
|---|---|
| 46 (pass 24) | blown: the figure's left vertical lies **on** the rod and its top stroke runs along the body outline. crackle: the lower run lies along the lower rod. **This is the complaint.** |
| 86 | a gap opens on all three; the top stroke has lifted off the body outline; full width kept |
| **96 — SHIPPED** | a clear band of pink shows between lightning and rod on blown and crackle; on hover the figure still reads as running around the loop. Size, jag, colour, density and the beaded inner filament all unchanged |
| 116 | **the first rung that draws attention to itself.** crackle: the figure straightens and re-lays itself ALONG the lower rod — a different placement. hover: the outer run becomes a bead chain climbing off into empty air. blown: the shape narrows |
| 130 | more of the same; on blown the figure has climbed onto the body |

96 is the rung below the first that restyles, and it is the one that reads best
**at native**, which is where the owner looks: `P25-LOOKS/05` shows pink between
the bolt and the band on all three clips where pass 24 showed contact.

**The lightning is not restyled, and that is structural rather than promised.**
`bolt_avoid_rods(int32_t pts[][3], int n, int lo, int hi)` takes only the point
array; radius, colour, gain, stamp density, morph clock, station identity and
topology are not in its scope and it cannot reach them. `msmooth` — the gate on
persistent lightning/particle identities and 60 Hz continuity — is green, which
is the evidence that a PATH moved and a figure was not switched.

### 1f. The declared side effect, judged

At Hover's tightest closure (f471) the push does carry part of the figure
outside the rods, and more clearance does make it more pronounced. The direction
predicted both and asked for a judgement.

* At **96** the outer run is a legible arc that still reads as energy running
  **around** the loop and returning through it; the pocket bar is intact.
* At **116** it becomes a detached bead chain in empty air and no longer reads
  as belonging to the antenna.

**That is the limit of the ladder, and it is why 116 was not taken even though
it is gate-clean.** `P25-LOOKS/04`.

### 1g. The split is RETIRED from the bank, and its control kept honest

Pass 24 measured depth-splitting a no-op — all 155 depth-straddling segments
were already drawn partly occluded at N = 1, because the unsplit bolt stamps
every 26.6 mm and the thinnest rod is 46 mm — and the isolated plate showed the
bolt still crossing. `kBoltSplitLiveN = 1`: **no subject ships it**, Inspect
included, which now takes the rollout's avoidance like everything else.

The mechanism stays as declared dead code with its knob, because the owner asked
for the comparison and the comparison's answer must stay reproducible. **A
retired mechanism whose control stopped being run is a control that dies
quietly**, so mbolt drives the split ITSELF on a named probe
(`kBoltSplitProbeSubject` / `kBoltSplitProbeN`): B2 still asserts the
mechanism's own contract — N times the sprites at 1/N of the spacing, measured
twice in one invocation — and `--fail-no-split` still reddens it. Both are in
the matrix.

---

## 2. ITEM 2 — THE EYE AMBIENCE, 600 → 800

`kEyeAmbientOrdinaryPm = 800`, **one** constant for every ordinary clip, because
that is what "everything" means and because twenty separate 800s would be twenty
places for the next tweak to go half-applied.

Laddered 600 / 700 / 800 / 900 / 1000 on Rest, four frames per rung at 8×, on
the frames a `framediff` named as the ones this knob moves most (f008, f264,
f272) — **not evenly spaced ones**. The crop was widened after the first attempt
because a tight one clipped the lens and made rim clearance unjudgeable.

| rung | what was seen |
|---|---|
| 600 | today: the stars drift; the owner's "subtle" |
| 700 | a touch more; separable from 600 only side by side |
| **800 — SHIPPED** | the stars plainly travel and change size; at f008 the right star comes NEAR the lens edge without reaching it |
| 900 | **the first rung that draws attention to itself**: at f008 the right star's arm sits ON the lens rim |
| 1000 | plainly a deliberate look, in a clip whose whole subject is resting |

800 is the rung below the first that oversteps — pass 24's rule, which the owner
did not revise; he asked for "a little stronger", and this is one rung.
`P25-LOOKS/06` and `07`.

**Localisation measured, not assumed** (committed `framediff.py`): 600 → 800
moves at most **217 px** on Rest's worst frame inside **x=171..241 y=159..176**,
and **276 px** on Taunt II inside **x=164..242 y=164..183**. Two lenses. Nothing
on the body, the band, the bolt or the motes.

**The per-clip gain is read through the SCHEDULE slot**, and that has a
consequence worth stating: `knead_schedule_slot(23) → 0`, so every subject built
on the fixed-camera idle — `manafold-crackle` **and `manafold-crackle-legacy`** —
takes **slot 0's** entry, and slot 23's is a defensively equal value that is
never read. That is how the non-live legacy control moved (§6, open issue 4).

**The contrast was checked deliberately AT 800**, not assumed to survive the
raise. `P25-LOOKS/08` lays Curious and Startle beside Rest at 800 and at 600:
the authored beats' stars are several times the size, swing across the whole
lens and change size beat to beat, while the ambient layer is a sliver drifting
inside a narrow lens. **Curious, Startle and Taunt III remain plainly the loud
ones**, they take gain 0, and their byte-identity under this item is asserted by
name in the matrix.

---

## 3. ITEM 3 — THE REAR-CALM LEVER: LADDERED, AND IT IS NOT THE LEVER

> ⚠ **SUPERSEDED, 2026-09-23, by `P25-BACKBALL.md`.** The owner approved one
> more targeted packet on this item. Everything below is still TRUE — the
> rear-calm lever does not calm the back ball, and the "52×" was a category
> error — but §3e's conclusion ("nothing short of lowering the antenna's
> upstream life would calm it") is **wrong**, and the reason it is wrong is the
> most useful finding either pass produced.
>
> A committed decomposition probe (`tools/reel/manafold_backball.cpp`) muted
> every rear authority one at a time on the posed result and found that the
> chain is a **travelling wave whose stations partially cancel** — freezing
> station B alone makes the last rod move **146 % MORE**. That is why every
> GAIN lever tried in four passes failed or backfired, including this one and
> including "removing the knead dip makes C travel further". The complaint is a
> FREQUENCY complaint, and it is answered by a **filter**: a centred zero-phase
> moving average on three named stations, which calms carrier C's travel by
> 30 % and its spin by 44 % while leaving the front ball's position untouched.
>
> It also found that the End swell — the thing this document and the codebase
> both call "the back ball" — is the **calmest back ball in the bank** and is
> not what the owner is looking at. See `P25-BACKBALL.md` §1.
>
> `kRearCarrierCalmPm` still ships unchanged at its authored value, and the
> `e-item3` / `e-item3-live` legs below still hold.

Laddered on Hover at **1000 / 800 / 700 / 600 / 500 / 420 / 300 / 150 / 0** —
the whole range, ending with the knob switched fully off.

### 3a. It does not calm the back ball. Measured, then looked at.

`mrear`, root-local, the posed skin ring at the C knuckle — the back ball is a
piece of SURFACE and mrear measures the surface, its own comment says why — over
every key and midpoint of the clip:

| rung | C path | vmax | amax | jmax | jrms |
|---:|---:|---:|---:|---:|---:|
| 1000 | 11269 | 39.0 | 16.36 | 17.45 | 6.665 |
| 500 | 11160 | 38.2 | 17.68 | 18.52 | 6.763 |
| 300 | 11136 | 38.8 | 17.88 | 19.25 | 6.682 |
| **0** | **11126** | **38.6** | 16.92 | 18.36 | **6.623** |

**1.3 % of path and nothing at all in the derivatives, with the knob OFF.**

**Looked at on CRACKLE**, which plays this same slot-0 choreography under a
**fixed** camera — so every pixel of movement is the creature and not the orbit.
(CLAUDE.md: a screen-space motion metric on an orbiting clip measures the orbit;
Hover orbits, Crackle does not, and it is the same animation. Using it here is
the committed fixed diagnostic camera the process notes ask for.) `P25-LOOKS/10`
lays six frames across the rear's busiest window at calm 1000 and at calm 0:
**same motion, both rows.** At native, four rungs side by side (`P25-LOOKS/12`)
are indistinguishable.

### 3b. THE "52×" WAS A CATEGORY ERROR, and it is worth writing down

Pass 24's reviewer measured this knob at **9,352 changed pixels** on Hover's
worst frame against the rear ambient's **178**, and read that as "roughly 52×
the lever". **Both numbers are correct and the inference is not.**

**Changed pixels between two configurations measures an OFFSET, not a MOTION.**
This knob shifts the whole inked rear band by a native pixel or two, and a
one-pixel shift of a large contoured shape repaints thousands of pixels; the
rear ambient moves a 20×16 patch. Neither number says anything about how much
the ball MOVES over the clip, which is the entire content of "too finicky and
moves too much".

This is the art law's failure mode in a new costume: a real measurement, quoted
for a question it cannot answer. The diff image (`P25-LOOKS/09`) shows what it
actually does — the loop and its sparks shift; nothing damps.

### 3c. Why no value here could have worked, structurally

The knob scales carrier C's **own** rotation (hinge play at C, the knead's grip,
out-of-plane and wag at C) plus nodule C's small translation. **A joint's own
rotation moves what is downstream of it; it cannot move the joint.** The back
ball IS at C, and its position is written by JunctionF, Neck, HingeA and HingeB
**upstream** — which are the antenna's life, bank-wide, and what the owner has
approved four directions running.

Everything else reachable was swept on the same measurement
(`P25-RECEIPTS/rearcalm-sensitivity-sweep.txt`):

| knob | C path | jrms |
|---|---:|---:|
| shipping | 11152 | 6.686 |
| rear ambient 400 (pass 23) | 11152 | 6.686 |
| rear ambient 0 | 11152 | **6.686 — EXACTLY zero effect** |
| knead dip clip 0 | 11419 (**higher**) | 6.440 |
| knead dent swing 0 | 11149 | 6.583 |
| knead dent depth 0 | 11159 | 6.471 |
| fold dip 0 | 11152 | 6.686 |

The rear ambient — the lever Direction 25 named and pass 24 built, which the
pass-24 reviewer already reported as not visible — moves carrier C by **exactly
zero**. That is consistent rather than surprising: it is the End carrier, one
joint further back. Nothing available moves the back ball by more than a few per
cent, and removing the knead dip makes it travel **further**.

### 3d. What ships, and the leg that keeps it honest

Every slot takes `kRearCarrierCalmPm`, so **item 3 changes no bytes.** The
per-clip table `kRearCarrierCalmClipPm[24]` and its
`ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM` ladder stay, because the knob is now real
in production — it was readable by one gate binary from pass 20 to pass 24 — and
a per-clip lever the owner can turn is worth more than a value chosen to look
like delivery.

**A knob that changes nothing and a knob that is DEAD look identical in a CRC,
and this exact knob was dead for four passes.** So the matrix asserts both:

* `e-item3` — at its shipped value the bank is byte-identical to pass 24, 22/22;
* `e-item3-live` — the same knob at `0:300,23:300` changes **exactly** hover,
  inspect and crackle, and nothing else.

Without the second leg the first is indistinguishable from the fault.

**Hover's front is untouched, as instructed. No other clip shows the fault**, so
nothing else was changed.

### 3e. What would actually calm it (for the owner, not done here)

Nothing short of lowering the antenna's upstream life — `kHingeAxisScalePm[1]`
and `[2]`, the A and B stations — which is bank-wide, would calm the whole
antenna on every clip, and is the opposite of what he asked for at the front. A
local answer would be a **new** mechanism (a damping/lag term on carrier C's
position), and Direction 26 says nothing new is introduced. It is open issue 2
and it needs his call.

---

## 4. GATES

### mbolt, now five legs and five controls

| leg | asserts | shipping reading |
|---|---|---|
| **B1 CLEAR** | a subject configured for avoidance has ZERO intersecting bolt segments, every key and midpoint | 0 on all 22 |
| **B2 SPLIT** | the split mechanism's own contract: N× the sprites at 1/N of the spacing, one clip measured twice in one invocation | 915,648 against 228,912 × 4 |
| **B3 ROLLOUT** | every live subject carries avoidance **and the whole bank's intersection total is zero** | 22 of 22, 0 |
| **B4 MIRROR** | mbolt's rows ARE `u02::kBoltLiveSubjects`, in order | 22 bound |
| **B5** | INFO: the closest approach anywhere in the bank, with its frame | not asserted — art value |

| control | what moved |
|---|---|
| `--fail-no-avoid` | **23 legs red**, 54,595 intersections — also the pass-23 baseline |
| `--fail-no-split` | B2's 4× ratio collapses to 1× |
| `--fail-fat-rod` | B1 red with capsules at 220 %: B1 reads the RODS, not a constant |
| `--fail-mirror-drift` **(new)** | B4 red. One row renamed so the lists disagree by one entry — **no legal stimulus can produce that**, so without it B4's silence stays an argument |
| `ZHAO_U02_BOLT_AVOID=off` + `--gate` | RC **1**. The positive control on the gate's own exit code |
| `ZHAO_U02_BOLT_CLEARANCE_MM=70` + `--gate` | RC **1**. The non-monotonicity in §1d is real and caught |

### TWO OF MY OWN GATE DEFECTS, FOUND BY THE LADDER AND REPAIRED

Both were found because the clearance ladder ran the gate at every rung, and
both are the shapes CLAUDE.md names.

1. **B3 printed "0 intersections remain in the bank" WHILE B1 WAS RED** with 11
   intersections on `blown` at 70 mm. Its first form summed the residual over
   the subjects carrying NO avoidance — an empty set after the rollout — so it
   was structurally incapable of seeing the case it was quoted about. A
   reassuring number from a counter that cannot look at the field that moved. It
   is the bank total now, and asserted, so B3 cannot be green while B1 is red.
2. **B2 went red at 96 mm because an ART VALUE moved.** Its spacing operand was
   measured only over segments within `kNearBandMm` of a rod, and a large enough
   clearance empties that set: spacing and reference both 0, leg red, mechanism
   perfectly fine. A gate whose operand a legal knob can delete is not a gate on
   the mechanism. B2 now measures the spacing over **every** bolt segment; the
   near-rod figure stays as INFO.

### Everything else, unchanged and green

mspan, msmooth, mprobe, mjointpub, mqa, mmeshcheck, moutline, mshell, mnodule,
meyesize, mrod, mrear (both modes) all pass at their unchanged thresholds with
every control still firing its own category.

`manafold_rear_audit.cpp`'s **private `atoi` of `ZHAO_U02_REAR_CARRIER_CALM_PM`
is removed.** That private parse WAS the original fault — from pass 20 to pass
24 it was the only parse of that variable anywhere, so the knob moved mrear's
reading of the creature and nothing that shipped. Pass 24 added it to the shared
parser; leaving the second door open would let mrear read one value while the
reel reads another, which is the same fault with the lanes swapped, and a
private `atoi` cannot see the new per-clip table at all.

---

## 5. BYTE IDENTITY

Measured on **all 22 live subjects**, per item, in one matrix invocation.

| configuration | result |
|---|---|
| all three items at their **pass-24** values | **22 / 22 byte-identical to pass 24** |
| every mechanism off | **22 / 22 byte-identical to pass 23** |
| item 1 alone (rollout + clearance) | all 22 change — which is what a rollout to all 22 means |
| item 2 alone (eye 600 → 800) | 19 change; **curious, startle and taunt III byte-identical** |
| item 3 alone | **0 change** — and the positive control proves the knob is live |
| shipping | the pass-25 bank, `P25-RECEIPTS/crcs-ship.txt` |

**Item 1's exact-off had to be a named selector, not an env flag.** Pass 24's
configuration was per SUBJECT — avoidance on hover and crackle, splitting on
inspect — and `ZHAO_U02_BOLT_AVOID=off` is bank-wide, so it reproduces pass 23
and not pass 24. `ZHAO_U02_BOLT_ROLLOUT=all|pass24` declares the predecessor as
data. **A rollout whose predecessor state cannot be re-rendered is a rollout
nobody can diff against.**

Exact-off knobs, all strict (RC 2 on anything malformed):

```
ZHAO_U02_BOLT_ROLLOUT=pass24
ZHAO_U02_BOLT_CLEARANCE_MM=46
ZHAO_U02_EYE_AMBIENT_CLIP_PM=0:600,1:600,...,23:600
ZHAO_U02_REAR_CARRIER_CALM_CLIP_PM=0:1000,23:1000
```

**No bound was relaxed anywhere.** No existing `constexpr` was lowered; every
new constant is new, and every one of them is a named editable knob with an env
ladder.

---

## 6. OPEN ISSUES

1. **`kBoltRodClearanceMm` is not monotone** (§1d). 56/66/70/76/106 mm leave
   intersections; 46/86/96/116/130/150/170 are clean. The cause is the fixed
   sweep count in `bolt_avoid_rods`. Raising `kBoltAvoidSweeps` would probably
   smooth it, and would move every clip's bolt path — not a final-pass change.
   The table is recorded at the constant and the gate catches it.
2. **The back ball is still finicky, and no available knob calms it** (§3c).
   The only levers are the antenna's upstream life (bank-wide, and the opposite
   of what he wants at the front) or a new damping term. **This needs the
   owner's call and it is the one thing this pass could not close.**
3. **Hover's tightest closure puts part of the figure outside the rods**, more
   so at 96 mm than at 46. Judged acceptable and legible (§1f); the lever back
   is `kBoltRodClearanceMm` downward, at the cost of the near passes returning.
4. **`manafold-crackle-legacy`'s "reproduces version-18 Wave D exactly" comment
   is stale, and has been since pass 24.** The identity legs cover the live 22,
   so this non-live control is outside them; it was checked by hand against the
   pass-24 binary anyway, because item 1 claims the name key protects the
   control subjects. It does: `ZHAO_U02_BOLT_ROLLOUT=pass24` +
   `..._CLEARANCE_MM=46` do **not** restore it. **Item 2 alone** moves it, and
   `ZHAO_U02_EYE_AMBIENT_CLIP_PM=0:600,23:600` restores it exactly — it reads
   slot 0's entry through the schedule map. Pass 24 introduced that layer at
   600 and broke the claim first; pass 25 moves it again. Reported, not
   silently repaired: exempting it needs a per-SUBJECT eye selector, which is a
   new mechanism on a final pass. The two form diagnostics (`manafold-still`,
   `manafold-nodule-solo`) **are** byte-identical, as designed.
5. The orthographic screen leg in mbolt's census stands as pass 24 wrote it; the
   gate does not rest on it.
6. **Not rendered here:** the 22-subject bank for publication, the encode, the
   merge and the deploy, by instruction.

---

## 7. EVIDENCE

⚠ **The BACK-BALL packet that followed this report has its own evidence set:**
`P25-BACKBALL.md`, `P25-BB-RECEIPTS/` and `P25-BB-LOOKS/`. Its matrix
(`gate-matrix-backball.txt`) carries every leg below forward and supersedes
`gate-matrix.txt` as the run's final gate record, and its
`P25-BB-RECEIPTS/crcs-backball.txt` is the bank that ships. `crcs-ship.txt`
below remains the EXACT-OFF reference that the packet's `e-bb-off` leg proves
it can still reach, bit for bit, on all 22.

| file | what it decides |
|---|---|
| `P25-RECEIPTS/gate-matrix.txt` + `gatematrix_p25.sh` + `runmatrix_p25.sh` | the matrix and its script, one invocation, run from a FROZEN copy |
| `P25-RECEIPTS/clearance-sweep.txt` | the non-monotonicity, whole bank, twelve rungs |
| `P25-RECEIPTS/mbolt-census-clearance46.txt` | the rollout's own census |
| `P25-RECEIPTS/rearcalm-ladder-mrear.txt` | item 3's nine rungs, carrier C |
| `P25-RECEIPTS/rearcalm-sensitivity-sweep.txt` | every other rear knob against the same measurement |
| `P25-RECEIPTS/crcs-ship.txt` | the pass-25 bank |
| `P25-RECEIPTS/binaries-md5.txt` | every binary |
| `P25-LOOKS/01`..`12` | the ladders and the A/B pairs this verdict rests on |
| `P25-NOTES/FINDINGS-01-clearance.md`, `FINDINGS-02-eyes-and-rear.md` | written at each look, before the next change |
| `P25-SHEETS/*` | every frame, production ink, of the clips the direction names |

**A provenance note, stated rather than left to be noticed.** After the matrix
ran, `manafold_boltgate.cpp`'s 100-line file header was still describing B3 as
pass 24's "CONTROL GROUP" and did not mention B4, B5 or the two repairs — a
comment asserting structure that had outlived the structure, which is the
gate-checklist fault this codebase keeps catching. It was corrected and **mbolt
alone was rebuilt** (`db85b53e…` → `b2bd4242…`) and **all seven of its legs and
controls re-run green** on the new binary
(`P25-RECEIPTS/mbolt-rerun-after-comment.txt`, and `mbolt-gate.txt` is that
run). **The renderer was not rebuilt**, so every bank CRC and every identity leg
in `gate-matrix.txt` stands against the binary that produced them.

**And the background task that ran the matrix reported exit code 1 while the
matrix reported `MATRIX_RC=0`, 240/240.** The 1 was my own trailing
`grep '^FAIL' gate-matrix.txt` — correctly finding nothing. CLAUDE.md's "read
the real exit code, not the pipeline's" in a third costume: the shell told the
truth about the wrong command. Recorded because a tired reader would have called
the matrix failed on it.

**Renderer:** `.tmp/p25/bin/zhao-reel-cel.exe`, MD5
`c4fba132fbadd8b02115caae3465dbba`, SHA-256
`6e96c375d03579cca952103ada20421194027dfcd5b8ec2ae2074c1fefcff7e3`. Built
directly with `tools/reel/build-direct.sh` and the `zhao-env.ps1` toolchain
(g++ 16.1.0 MinGW-W64 ucrt), clean tree, RC read directly and not through a
pipe. No CMake was used and no CMake result is claimed.

**Presentation:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, set
explicitly on every render and every gate in this report.
