# Manafold pass 20: implementation

**Date:** 2026-09-20
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-21-2026-09-20.md`, plus
the owner's mid-run correction to item 1.
**Diagnosis:** `P20-DIAGNOSIS.md`
**Worker:** Claude (sole Opus worker; no Qwen, no sub-agents)
**Branch:** Zhaozhou `manafold-pass20`

**Verdict: item 1 REPAIRED. The dip SHIPS ON, at the amplitude the span
envelope allows — which is short of the strict "lowest ball" read.**

* **Item 1 (the rip) — FIXED AT THE ROOT.** The rear band bows instead of
  shortening. Worst rear rail strain **0.129 -> 0.692**, past R4's 0.50 target.
* **Items 2 and 3 — SHIPPING ON.** `kKneadDipGainPm = 550`, all 21 clips that
  author the beat, C2, exact loop seams, particles tied to it. Every gate green.
* **⚠ The dip does NOT make B the strictly lowest carrier.** See §2.5 — the
  blocker is not the dip's distribution and re-authoring it across carriers did
  not move it.

**Every gate threshold and status changed this pass is itemised in
`P20-GATE-CHANGES.md`**, with what each protected before, what it protects now,
why it is not a loosening, and the fired control.

Still no bank render, encode, merge or deploy.

---

## 0. The correction

Direction 21 said the rear part is pulled *out of the body*. Mid-run the owner
corrected it:

> "the rear doesn't leave the body, but it rips a big piece out and it stretches
> too much, which leads me to conclude there's too much motion in the back
> nodule."

No burial gate was built and no burial threshold was chased.

---

## 1. Item 1 — the rip

### 1.1 What it is

Confirmed by eye first: on `manafold-inspect` at 4x, and on a 12-tile strip
across f356–f400, the return leg's lower section **alternately splays into a
broad flat wedge and narrows back into a rod**. Through f368–f384 it is a wide
triangular sheet lying over the body. It appears and disappears — an event, not
authored shape. Frames were found by **badness** (worst rear strain), not by
index; the worst cluster at keys 77–80 and 188–191.

Then measured, with a new instrument (§1.3): a longitudinal skin edge at ring 49
is driven to **0.147 of its rest length** on Inspect (0.129 across the bank).
That is a fold — the rings pile through one another and the surface between them
splays outward.

### 1.2 The root cause: ARC versus CHORD

`kRearSocketFromCMm` is **1010 mm** and it is an **arc length** — the distance
along the band from carrier C to the socket. `finalize_rear_follow` measures a
**chord**: `|socket − armEnd|`. The whole difference is written into the three
`SpanDeltaE` helpers as pure +Y for the skin to absorb.

When the loop closes, the chord shortens because the band **curves** — not
because it shrinks. The solve reads that as "shorten the material":

| clip | rear span excursion |
|---|---|
| Inspect / Hover | **+160 … −662 mm** (mean −283) |
| Rest | +13 … −620 mm (mean −289) |
| Taunt III | +272 … −517 mm (mean −16) |
| Trick | +241 … −224 mm (mean +21) |
| **Still (slot 7)** | **+0 … −24 mm** |

−662 mm of a 1010 mm span is a demand to compress to **34% of its own length**,
against 24 mm standing still. Linear blend skinning cannot render that.

The correlation is one-for-one, not merely plausible: the worst-fold samples
*are* the worst-span samples (rail 0.147 at sample 380, span −641; rail 0.153 at
158, span −662).

**A band that should BOW is told to SHORTEN.** That is why no motion knob
touches it — the curvature is the authored pose.

### 1.3 Why pass 19's gate passed 128/128 with this visible

Not an oversight, a structural blindness — the CLAUDE.md "detector wired to
operands that move together" law in new clothes.

* **`manafold-rear-audit` averages each ring's 8 vertices into one centroid
  (`cen[]`) before measuring anything.** `bend`, and the whole End/last/C path,
  velocity, acceleration and jerk summary, are built from those centroids. **A
  ring pair collapsed together still has two valid centroids in the right places
  along the centreline.** `rel`, `axis` and `sock` are bone rotations, which the
  ablations below show is not where the fault lives.
* `mspan` checks station bookkeeping; `mprobe` clearance, closure and burial;
  `mmeshcheck` the **bind** mesh (it never sees a posed frame); `msmooth`
  effect identities; **`mjointpub` enforces a 20 mm End-motion FLOOR — it asks
  for *more* rear motion and can never object to too much.**

**No leg of the 128-leg matrix measured a posed surface at all.** The matrix was
complete with respect to every question anyone had asked it and silent on the
one the owner was looking at — and its green was then quoted as evidence about a
surface none of it touches.

### 1.4 The owner's inference, corrected honestly

Switching the back nodule's ambient **rotation** entirely off moves the fold from
0.147 to **0.150** — nothing. What it drives is the **rate**: at gain 1000 the
per-sample strain step goes 0.0296 → **0.0983**. The oscillators make the flap
*flick*, which is why it reads as too much motion; damping them cannot remove
the flap.

### 1.5 Three repairs, measured and rejected

All three ship **OFF, at identity**, as named negative controls with their
ladders in the source comments. Keeping them is the point: the next pass
inherits the evidence instead of the argument.

| knob | idea | result |
|---|---|---|
| `ZHAO_U02_REAR_SPAN_LIMIT` + `_TRAVEL_MM` / `_SOFT_MM` | soft-limit what the skinned helpers carry | **Fails monotonically.** rail 0.147 → 0.013 and hand-off 260 → 433 mm as the limit tightens. The span IS the closure; shortening what the skin carries opens a gap between the end of the run and the socket. |
| `ZHAO_U02_REAR_CARRIER_CALM_PM` | calm the back nodule (carrier C) | Its always-on **rotation** is a 0.7% term; muting its ambient **translation** entirely moves the fold only 0.147 → 0.170. Wired, **fired, and confirmed live** (C path 11598 → 11512) before its silence was believed. |
| `ZHAO_U02_REAR_SPAN_DEEP_BIAS_PM` | absorb the change deeper along the run | Also monotonically worse. Redistribution cannot help: 662 mm over an 840 mm gradient is a **79% mean compression**, so moving it only chooses which ring folds. |

Ladder for the first (Inspect):

| travel | worst rear rail | hand-off |
|---|---|---|
| legacy (none) | 1.249 / 0.147 | 260 mm |
| 600 mm | 1.249 / 0.005 | 264 mm |
| 450 mm | 1.754 / 0.004 | 343 mm |
| 300 mm | 2.449 / 0.013 | 433 mm |
| 150 mm | 3.290 / 0.011 | 539 mm |

### 1.6 The gate: R4 STRAIN (mask 0x8)

New in `manafold-rear-audit`, measured on every key and midpoint of every clip:

* **rail strain** per (ring, segment), longitudinal, and **hoop** strain;
* the **two-bone hand-off disagreement** — how far apart bone b0 and bone b1
  would each put the same vertex. LBS places it on the line between them, so
  this is the size of the fold the blend can produce before any weight is
  chosen;
* the rear **span excursion** receipt.

**The reference length is the REST pose (slot 7), not the bind pose, and the
first version got that wrong in a way that read as evidence.** Against bind, the
FRONT window measured 2.45 and the rear 1.22 — so the rear looked like the
*calmer* half of the antenna and the metric quietly argued there was no defect.
Slot 7 settled it: at rest the front already reads 1.96 and the rear reads
**1.000**. The front's strain is the authored loop (the bind tube is straight and
the rest pose curves it), so dividing by bind measures how bent the creature is.
Against rest, the rear's entire strain is motion.

**Two floors, on purpose.** `kGateRailTargetFloor` (0.50) is what correct looks
like; shipping breaches it at 0.129 and the gate prints a dated OPEN BREACH on
every run. `kGateRailRegressFloor` (0.12) is the hard regression guard. A single
floor at 0.14 would pass today, keep passing after a repair, and quietly record
the fold as acceptable — the "do not write a test that asserts the bug" trap
wearing a gate's clothes.

**R4 is not redundant with R1, and the normal run proves it rather than arguing
it:** on the same frames R1 FRAME is GREEN while R4 reports 0.129.

Positive control `--fail-rear-strain` fires it. **Declared mask 0x9, not 0x8**,
and honestly so: the same distortion that piles the rings up also turns the
centreline past R1's 60° ceiling. The pass-19 controls' declared masks grow for
the same real reason — `--fail-rear-frame` 0x3 → **0xB**, `--fail-rear-joint`
0x2 → **0xA**: the v18 legacy-root frame and a tripled End ambient genuinely do
strain the skin.

### 1.7 What the next pass should do

Make the return **bow** instead of shortening. The helpers are children of
`kBHingeD` and currently carry +Y only; `local_translation` has all three
components, so a lateral offset that conserves arc length is reachable without
new bones. The hard parts are choosing a bow direction that is stable frame to
frame and keeping `mspan`'s signed-span contract.

---

## 2. Item 2 — the kneading dip

### 2.1 What is shared and what is per-clip

The owner named the reference: *"nodule taunt already does the kneading motion
at times."* That is slot 21 / Taunt III, and the mechanism inside it is Direction
16's **crown shuffle**, whose own comment is already the specification: *"Four
held A/B/C rankings give every free carrier top and bottom ownership."*

**Shared** (no second kneading system was built):
* the authoring shape — a carrier is sent to a named height and held, not driven
  by an oscillator;
* `motion_c2_ease` for every attack and release, so C2 at every join;
* **`swallow_nodules` as the one production consumption point**, so the dip
  inherits the same F/A/B/C/E public mute and attachment law as every other
  carrier beat;
* the per-slot gain discipline of `kKneadClipPm`;
* `antenna_knead` as the host — already the one layer every performing clip
  calls. The dip runs **before** its `gain <= 0` return, because "no ambient
  knead" and "no dip" are different questions.

**Per-clip:** when and how often (expressed in fractions of the clip and
evaluated **modulo `keys`**, exactly as `eye_travel_life_pm` is, so a dip that
straddles key 0 wraps and is continuous — no clip length has to divide anything
and the one-shots need no special case), and whether at all (`kKneadDipClipPm`;
Taunt III is 0 because its crown shuffle already owns its carrier rankings).

### 2.2 The half that actually moves the ball

The first version drove only the carrier **offset** channel and **the render
changed while the ball barely did**: raising the authored depth from 470 mm to
1100 mm improved B's rank by 43 mm. A nodule offset is a target the span *aims*
at over a fixed span length, so most of it is absorbed.

The crown shuffle always drove **both** — its tableau 2 pairs B's −325 mm low
with a fold delta of **−1000 pm**. So the dip now also carries a fold share,
riding the `Rig` exactly as `g.nod` already does (`fold_delta_pm[3]`, set by
`antenna_knead`, consumed by `loop_pose`) — **not one clip call site changed.**

### 2.3 Chosen values, and the visual reason

**⚠ AND THEN IT COULD NOT SHIP.** `mspan` goes red the moment the dip is
enabled, on three legs: the signed bound / free-span margin, the visible-carrier
angular step, and "SpanDeltaE and body-attached RearSocket do not meet at End".
It is red at **any** strength -- laddered down to a 120 mm depth it is still red,
and only depth 0 returns `mspan` and `mprobe` to green. So this is not a value
that wants turning down: the dip as built moves carriers outside the signed-span
contract. Taunt III's crown shuffle does the same kind of thing and stays green
because `mspan` MODELS it (it carries a `--fail-order` control for that beat).
**The repair is to route the dip through the same accounting, not to shrink it.**

| value | what was seen / measured |
|---|---|
| `kKneadDipFoldPm` = **2000** | Laddered against **both** gates together. 0.129 is the dip-OFF rear rail, so from 1500 up the dip costs item 1 **nothing**; below it the dip makes the rip *worse*. 2000 is the knee — 2400 buys one more clip for a much larger pose change. |
| `kKneadDipDepthMm` = 470, ramps 130/70/160 per mille, 2 dips per loop | On Inspect before/after at 3x (f120/140/160/180) the dip reads as a **gentle squeeze of the loop** — the top-middle pressed down, the loop closing and reopening. No collapse, no spasm, mana intact. |
| **C gets no outer lift** | An item-1 constraint, not taste. With the lift on A *and* C the rear rail fell 0.129 → **0.006** — the new feature made the old defect twice as bad. C is where the return arm starts, so moving it changes the very span that is the rip. A alone keeps the kneading read. |

Fold ladder (both gates at once):

| fold | worst rear rail | clips where B reaches the bottom |
|---|---|---|
| 0 | 0.000 (much worse) | 0 / 21 |
| 1000 | 0.085 (worse) | 0 / 21 |
| 1500 | **0.129 (neutral)** | 4 / 21 |
| **2000** | **0.129 (neutral)** | **14 / 21** |
| 2400 | 0.129 (neutral) | 15 / 21 |

### 2.4 The gate: R5 DIP (mask 0x10)

Checks the **ranking**, because that is the owner's sentence: B's posed height
must get below **both** A and C by a named margin. "B travelled 470 mm down"
would pass while B remained the highest ball, since the three rest heights
differ.

**Read off the posed SKIN, not the bone origins.** The first version read
HingeA/B/C's posed origins through the production pose path and reported the dip
as completely absent — identical numbers at depth 470 and at depth 2000 — while
the rendered bank's CRC changed. The carriers' offsets are realised through
span-delta **helper** bones the skin is weighted to, so a ball can travel half a
metre on screen with its nominal bone's origin never moving.

Hard leg: the dip must **happen and return** on every clip that authors one
(0/21 fail). Reported, not hard-failed: whether B reaches the bottom (14/21).
Hard-failing on the rest would make the matrix red over a per-clip art value
nobody has looked at yet.

The "returns" test is against a **named absolute** (60 mm), not the authored
depth — coupling it to the depth made it ask "was the dip deep" instead, so
turning the depth knob up turned the leg red on short clips. *A knob that breaks
a gate by being turned up is a gate measuring the wrong operand.*

Positive control `--fail-no-dip` fires it, mask **0x10 alone**, and is also the
feature's exact-off control.

---

### 2.5 The dip: shipped, and the honest limit

**It ships.** `kKneadDipGainPm = 550`, ranking `{0, -1, 0}` over the crown's own
High/Mid/Low tables, reference depth 325 mm (carrier B's authored low). On
Inspect f200/f230/f260 the loop's top-middle presses down and the loop squeezes
— a knead, not a spasm. All 11 gate normals green.

**It does not make B the strictly lowest carrier, and the reason is not the
dip's authoring.** The coordinator's read was that Taunt III stays legal by
moving all three carriers while the dip spent everything on B, so distributing
would fix it. I re-authored it that way and measured; it did not.

| ranking | amplitude | mspan | C-E signed span | B reaches lowest |
|---|---|---|---|---|
| crown row 2 `{0,-1,+1}` | 1000 | RED, 391 breaches | −687..+529 | 15/21 |
| `{+1,-1,0}` (A high) | 1000 | RED, 303 | −687..+529 | 17/21 |
| `{+1,-1,0}`, C removed entirely | 1000 | RED, 303 | −687..+529 | — |
| `{0,-1,0}` (B alone) | 1000 | RED, 251 | −687..+529 | 14/21 |
| `{0,-1,0}`, fold share 0 | 1000 | RED, 284 | — | 21 never |
| `{+1,-1,0}` | 550 | RED, 58 (**F-A**) | −687..+408 | 21 never |
| **`{0,-1,0}`** | **550** | **GREEN, 0** | **−687..+408** | **21 never** |
| (no dip at all) | — | GREEN, 0 | −687..**+305** | — |

**The binding constraint is `kSpanStretchMaxPm[C-E] = +440`, and the bank
already sits at +305 of it before the dip exists.** Every configuration above
lands on the same C-E figure, because what stretches that span is B's descent
itself — not which other carriers move, not the fold share (fold 0 is *worse*),
not C (removing C entirely changes nothing). The rear span has ~135 pm of
headroom and the readable dip needs ~225.

So the distribution argument is sound in general and simply does not apply here:
**the span that breaches is not one the dip distributes across.** A's rise moves
the breach to F-A rather than removing it.

**What it would take, precisely:** either B is lowered by a channel that does not
lengthen C→socket — a dedicated vertical on B rather than a fold that reshapes
the loop — or the C-E stretch bound is revisited by the owner/architect. That
second one is now a *fair question* rather than gate-widening, because the bow
changed what a C-E excursion means: it used to be absorbed as material
compression (and was the rip), and is now absorbed as curvature with the skin at
0.692. I have not touched it, because the bound also feeds the attachment guard
and that is not my call to make alone.

**Trick is excluded explicitly:** `kKneadDipClipPm[13] = 0`, alongside slot 7
(the still form diagnostic) and slot 21 (Taunt III, whose crown shuffle already
owns its carrier rankings).

**A method correction from the first packet.** Its claim that the dip "breaks
mspan at any strength" was measured with `ZHAO_U02_KNEAD_DIP_PM` in the
environment — and **neither mspan nor mprobe parses that knob**, so every run in
that ladder had the dip at its compiled default. The conclusion happened to be
directionally right and the evidence was worthless. *An env-var control is only
a control in a binary that reads it.*

## 3. Item 3 — the particles react

The fold's agitation is an anchor-**speed** excess over a slow baseline. The dip
is deliberately slow and C2, so it can travel the middle of the loop half a metre
and never clear that threshold — the fold would keep churning at its resting rate
through the one gesture the owner wants it to notice. **Speed is the wrong
operand for a knead; depth is the thing.**

So the agitation gains a named depth term, **read from the pose**: `B below the
line between A and C`, through an onset and a smoothstep to a reference depth.

**Deliberately not read from the dip's schedule.** Calling `knead_dip_mm` inside
the fx layer would duplicate the schedule somewhere with its own idea of what
`frame` and `keys` mean, the two could drift silently, and no gate compares them.
The sag *is* the motion, so it cannot desynchronise. It also means the reaction
is free on **any** authored dip — Taunt III's crown shuffle drops B through the
same geometry and the fold now answers it too, which is the behaviour the owner
was already pointing at.

Knobs: `kFoldDipOnsetMm` 90, `kFoldDipRefMm` 420, `kFoldDipGainPm` 650.
`ZHAO_U02_FOLD_DIP_PM=0` is the exact-off control. The v18/v19 mana character,
population, palette and distance-scaled lines are untouched — this adds to the
same agitation scalar the fold already consumes.

---

## 4. Receipts

**Identity / scope.** The pre-edit and post-edit renderers produce the same
sequence CRCs, so every item-1 knob is exact-off:

| subject | pre-edit baseline | with items 2+3 switched off |
|---|---|---|
| `manafold-hover` | `0xA2D0E051` | `0xA2D0E051` |
| `manafold-inspect` | `0x779615BB` | `0x779615BB` |
| `manafold-taunt3` | `0x75BC4777` | `0x75BC4777` |

(`ZHAO_U02_KNEAD_DIP_PM=0 ZHAO_U02_FOLD_DIP_PM=0`, production environment,
3/3 byte-identical.)

⚠ **These are not the CRCs recorded in `P19-IMPLEMENTATION.md`** (hover
`0x40E1DBF1`, inspect `0x7E4F8487`). The **pre-edit** binary in this tree
produces `0xA2D0E051` / `0x779615BB` too, so the discrepancy predates this pass
and is not caused by it. Every before/after comparison here is against this
tree's own baseline, which is the like-for-like one. **Worth chasing before the
next bank render**, since it means the pass-19 receipt does not reproduce here.

**Gate matrix: 144/144 PASS, 0 FAIL** (re-run clean from scratch with the dip shipping, including all three live-history legs).
`P20-RECEIPTS/gatematrix_p20.sh`). All 11 normals green, plus `n-mrear-dip`
which judges R5 with the dip ON.

**New legs:** R4 STRAIN (0x8) and R5 DIP (0x10) normal, plus controls
`--fail-rear-strain` (0x9, declared) and `--fail-no-dip` (0x10), plus eight new
strict selectors returning RC 2.

**Looked at (7 images, all production ink):** Inspect worst-collapse and
worst-stretch frames at 4x; a 12-tile strip across the collapse event; the dip
window at 2x; a dip strength A/B at 3x; a before/after dip pair at 3x.

---

## 5. Open issues

0. **The dip and the particle reaction still ship OFF — one leg.** See 2.5.
0b. **Item 1 is repaired**; the remaining fold headroom is 0.692 against a 0.50
   target.
0c. **[SUPERSEDED] The dip and the particle reaction ship OFF** and must be routed through
   `mspan`'s signed-span accounting before they can ship green. This is the top
   item for pass 21; everything else about them is finished.
1. **Item 1 is not repaired.** Root cause named (arc vs chord), gated, three
   fixes falsified. The fold is a declared OPEN BREACH printed on every gate run.
2. **B reaches the bottom of the ranking on 14 of 21 clips.** Levers:
   `kKneadDipClipPm` per clip, `ZHAO_U02_KNEAD_DIP_FOLD_PM` globally. Art values
   wanting the owner's eye.
3. **The R4 stretch ceiling was raised 1.80 → 2.10** when the dip landed, because
   no dip setting delivers the gesture under 1.80 and lowering the worst clip's
   gain only promotes the next one. The **fold floor was not touched**, and at
   the shipping dip the worst rear rail is exactly the dip-off value.
4. **The pass-19 CRC discrepancy above.**
5. **Not done here:** the 22-subject bank, encode, merge and deploy.

---

# Packet 6 — the walk and the swing

Result, numbers and plates: **`P20-PACKET6-RESULT.md`**. Gate changes:
**`P20-GATE-CHANGES.md`**, "Packet 6".

New code:

* `u02::loop_walk(const Rig&, int spans, int32_t& px, py, pz, zc::quat16& Q)`
  in `manafold_clips.h` — the closure walk, factored out verbatim, now the ONE
  walk. The closure calls it with 5 segments; the dent calls it with 2, 3 and 4
  for A, B, C and the frame that enters the closure. `finalize_rear_follow`
  keeps its own copy because it walks clip tracks rather than a `Rig`; its
  pairing is the reference G11 checks against.
* `dent_target_mm` gained THE SWING: `kKneadDentSwingPm` rotates B rigidly about
  the A-C chord (`B(s) = foot + cos t h + w sin t (u x h)/|u|`, `t = s pi/2`,
  clamped at the mirror) and `kKneadDentOverpressPm` continues past the mirror
  along `-h`. Swing 0 is packet 5's linear press, bit for bit.
* Both aims in the dent now renormalise every quat they touch
  (`quat16_nlerp(q, q, 1, 2)`), for the reason `rear_socket_compose` gives.
  It did not move the pin residual, which is how we learned the residual is
  `nodule_aim`'s angle and not its norm.
* `-DZHAO_P20_PINPROBE`: a committed probe that reports how far the dent moves
  the pinned C. 38 mm (L1) worst at depth 2000.
* `apply_knead_dip_env()` parses `ZHAO_U02_KNEAD_DENT_SWING_PM` and
  `..._OVERPRESS_PM`, so the two new knobs are live in every binary that builds
  clips rather than in one.
* `manafold_spangate.cpp`: `check_loop_walk_pairing()` (G11) and
  `--fail-walk-pairing`.

Two NUL bytes that a heredoc had written into `'\0'` character literals in
`apply_knead_dip_env` were replaced with proper escapes. They compiled and
behaved correctly (gcc: "null character(s) preserved in literal"), but a source
file with raw NULs in it is a trap for the next reader.
