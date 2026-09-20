# Manafold pass 20: implementation

> WARNING: **READ THE `CLOSE` SECTION AT THE END OF THIS FILE FIRST.** It
> supersedes every figure here that was measured under an override, and names
> each one in a table. The pass-20 review found that `mrear --gate --dip` set
> the dip gain to 1000 while the tree shipped 550, so the headline counts below
> describe a creature nobody renders.

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

---

# Packet 7 — the roll-stable aim, and the dip ships

Result and receipts: **`P20-PACKET7-RESULT.md`**.

* `u02::shortest_arc_from_y(vx, vy, vz)` -- the minimal rotation taking +Y to a
  target direction, as (L + vy, vz, 0, -vx) normalised into quat16. No
  trigonometry, no accumulated twist, and the antiparallel case takes a NAMED
  axis (+Z, the loop's fold axis) rather than whatever the arithmetic produces.
* `u02::nodule_aim_rollstable(...)` -- nodule_aim's bookkeeping with that
  correction. A SEPARATE function: the production nodule solve still calls
  nodule_aim verbatim, so it is exact-off by construction rather than by a flag.
* `kKneadDipSolver = kDent` and `kKneadDentDepthPm = 2200`: the dent ships.
* `kKneadDipClipPm[20]` 750 -> 900 (blown), the per-clip lever this table exists
  to be, which took the last gameplay clip to strictly lowest.

The particle reaction needed no work: manafold_fx.h reads B's sag from the POSED
rig, not from the dip's schedule, so it followed the solver change by itself.
`ZHAO_U02_FOLD_DIP_PM=0` still reverts the mana to its version-18/19 bytes
(inspect 0xE5C1D75B against the shipping 0xFD8D4D0E).

---

# CLOSE — the review's block, cleared

**This section supersedes every figure above that was measured under an
override, and it names each one.** The rule it enforces: a gate reads the
shipping constants; it does not choose them, and a number quoted from a run that
moved one is not a shipping number.

## 0. Corrections to the figures above

| where | said | truth, at the shipping constants |
|---|---|---|
| packet 7 §3, and "B reaches the bottom of the ranking on 14 of 21" in the packet-6 status list | 19 of 21 / 14 of 21 | Both came from runs at a dip gain of **1000** while the tree shipped **550** (`mrear --gate --dip` overrode the gain). At 550 with the packet-7 aim it was **4 of 21**. The number that ships now is **19 of 19 hosting clips**, measured with no override at all — see §3. |
| `-DZHAO_P20_PINPROBE`: "38 mm (L1) worst at depth 2000" | 38 mm | Measured through the *pre*-roll-stable aim. The review re-measured it at **7 mm**; at the values that ship now it is **8 mm (L1) and 0.066° of frame**, and it is a gate leg rather than a private define — see §5. |
| packet 7: "`kKneadDentDepthPm = 2200`… the depth was chosen to fit the ceiling" | 2200 | The *direction* of that trade was right and the *number* was the roll flip's, not the gesture's. With the flip repaired the same 8° ceiling admits **4200** — see §1 and §2. |
| packet 7: "the particle reaction needed no work" | needed no work | It needed all of it: 588 of 600 Hover frames were byte-identical to the reaction switched off. Re-authored — see §4. |
| packet 7: `kKneadDipClipPm[20]` 750 → 900 | — | Superseded. The whole table was re-trimmed one clip at a time — see §3. |
| gate-change 3: hand-off "hard bound 320 mm → REPORTED ONLY" | reported only | Re-bounded at **420 mm** for the new solve, with a fired control — see §5. |
| gate-change 4: the stretch ceiling rise credited to "the dip ladder (1.919)" | the dip | The review measured it: with `KNEAD_DIP_PM=0` the bank still reads 2.052. **The bow raised it, not the dip.** Unchanged here; recorded so the attribution stops being repeated. |

## 1. The roll flip — cause, and a repair by construction

The reviewer's ladder reproduced exactly: G9 worst angular step **7.772** at gain
550, **11.283** at 750, **50.963** at 1000, with the position step flat at
72.30→72.36 mm. A carrier whose position moves smoothly while its frame jumps 51°
in one 60 Hz sample is a rotation fault, not a motion one.

**It is not the named flip branch.** `shortest_arc_from_y`'s exact-pole guard
(`x == 0 && z == 0 && y < 0`) is never entered. It is the branch's
*neighbourhood*, and the mechanism is **quantisation, not discontinuity**:

* the shortest arc is `q ∝ (|v| + v_y, v_z, 0, −v_x)`, whose vector part is
  `a × v`;
* the dent presses B along its perpendicular to the A–C chord, which drives the
  A→B segment toward **antiparallel with where it already points** — and there
  `|a × v| → 0`;
* the axis direction is then recovered from `(v_z, 0, −v_x)`, two **millimetre
  integers** that have both collapsed to a handful of counts. The axis is
  quantised to tens of degrees, and a ~180° turn about an axis tens of degrees
  wrong is an orientation tens of degrees wrong.

In exact arithmetic the rotation is continuous through that neighbourhood. In
int16 lanes it is not, and no amount of care in the *branch* helps, because the
fault is in the approach.

**The repair is to stop deriving the axis from the thing that vanishes.**
`u02::arc_from_y_about(v, u)` takes the turn as an **angle about a named axis**:

* the axis is the normal of the **pre-dent (A, B, C) triangle**, computed once
  per sample from the carriers as the ambient pose left them. It has **no term
  in `dent_pm` at all**, so it cannot move when the beat deepens — it is the
  continuous reference the beat itself defines, not a constant chosen at one
  instant;
* `|N|` is `|AB||AC|sin(ABC)`, ~10⁵ mm², and never collapses;
* the half angle comes from the production `quat_axis` (`fx_sin`/`fx_cos`), so
  it is never recovered from a near-zero norm. A 179.9° turn is as accurate as a
  5° one, and there is no ill-conditioned region left to stay out of;
* the residual out-of-plane component is finished by the old shortest arc, which
  is well conditioned once the big angle has gone. (The shipping dent is the
  rigid **swing** about the chord, so B does leave the triangle's plane; the
  normal is a conditioner, and the measurement below is what chose it.)
* `nodule_aim_rollstable` takes the world axis **append-only**. With no axis it
  is the old function byte for byte, so nothing else in the tree moves.

**The chord was the other candidate and it measured worse.** The chord is the
swing's own rotation axis, so it looked exactly right on paper — but it is not
perpendicular to the segment, and the function's +Y-component drop mangles it.
Measured on the same ladder: 9.773° at depth 2200 and 164.5° at 6000, against the
triangle normal's 7.591 and 15.9. Measurement on the **comparison** side chose
between two authored candidates; it did not pick a value.

| configuration | OLD aim | NEW aim, shipping build |
|---|---|---|
| gain 550 / depth 2200 (was shipping) | 7.772 | **7.591** — the dip-off bank's own worst; the dent is invisible to the gate |
| gain 550 / depth 3400 | 9.928 **FAIL** | **7.591** — still invisible |
| gain 550 / depth 4200 (**ships**) | — | **7.642** — the dent's whole cost is 0.051° |
| gain 550 / depth 4800 | — | 8.198 FAIL — the ceiling is reached at about 4750 |
| gain 1000 / depth 2200 | **50.963 FAIL** | **7.591** |

The last row is the review's own worst case, configuration for configuration.
The flip is not smaller; it is gone. (`P20-RECEIPTS/close-gain-ladder-newaim.txt`
and `close-override-audit.txt`; the depth rows are taken at the shipping
per-clip shares and every run prints its own `CONFIG JUDGED` line.)

### 1a. "Does any other aim path still compose z-then-x?" — yes, four, and here is why they are not at risk

The brief asked, so it was checked rather than assumed. `grep` over `tools/reel`
finds the z-then-x composition at five live sites:

| site | what it aims | status |
|---|---|---|
| `nodule_aim` (three callers) | the AMBIENT nodule solve for carriers A, B, C | untouched, deliberately: it is the pass-11 pose the whole bank is byte-compatible with, and the dent runs *after* it |
| `loop_pose`'s HingeD closure (2 sites) and `finalize_rear_follow`'s two HingeD writes | the return arm's last segment, at the re-entry anchor and at the rear socket | untouched, pre-existing |

**Why the HingeD family is not the same hazard.** The quantisation fault needs
the aim's target to approach **antiparallel with where the segment already
points**, which is what collapses `a × v`. HingeD aims at a fixed anchor deep
inside the body, from a segment that already points at it; the turn is small and
its cross product never collapses. The dent cannot push it there either, because
HingeC is **pinned** — the closure walk enters at exactly its pre-dent frame,
which is the contract G10 now gates (0.066° of drift).

**What would show it if that ever changed**, so the next reader has a check
rather than an assurance: mspan's G6 bounds the worst consecutive-step turn on
the posed rings (shipping **66.43°** against a 140 ceiling) and G7 bounds the
closure endpoint (**6.065 mm**, straightness sin 0.0077). A HingeD roll flip
would show in both. Both are green and neither moved this pass.

**`nodule_aim`'s three callers are the interesting residual.** They are exact-off
by construction today, but they are the same function with the same
near-antiparallel blind spot, and a future beat that drives an ambient carrier
offset hard enough would hit it. `arc_from_y_about` is available to them
append-only — the axis argument is all it needs. Not done here, because changing
them moves every byte in the bank and this pass had a block to clear.

## 2. What capped the depth next, and the second repair

With the flip gone, G9's worst moved off carrier B and became an honest **rate**
on carrier A. The per-slot angular trace (`mspan --motion-csv`) shows the whole
beat on the limiting clips as **one smooth symmetric 16-frame hump** — no spike,
no flip — peaking at exactly the rate `kKneadDipMinRampKeys = 9` sets.

Nine keys is 0.3 s at 30 Hz. That is a jab, not a knead, and the clips it bound
were precisely the short ones whose ramps sit on the floor rather than on the
130‰ fraction. Scanning the floor against "how many hosting clips can reach the
ranking inside the unmoved 8° ceiling":

| ramp floor (keys) | hosting clips that can reach B-lowest | the clip that cannot |
|---|---|---|
| 18 | 17 / 19 | 3 and 20 |
| 20 | 18 / 19 | 3 |
| 22 | 17 / 19 | 3, 11 |
| 24 | 18 / 19 | 11 |
| 26, 28 | 18 / 19 | 8 |
| **30** | **19 / 19** | — |
| 34 | 19 / 19 | — |

The rungs between 18 and 28 each lose a *different* clip because the dip COUNT
flips between two and one at different clip lengths; 30 is where every clip is on
the same side of that flip. **30 keys ships** — one second of press, and on a clip
under ~230 keys one deliberate knead instead of two fast ones, which is the
gesture the owner named rather than a compromise made for the gate.
`win_keys` already clamps every window at `span/3`, so the floor can never push a
short clip's window past its own length.

## 3. B strictly the lowest carrier — the shipping figure

`kKneadDentDepthPm` **2200 → 4200**, `kKneadDipMinRampKeys` **9 → 30**, and
`kKneadDipClipPm` re-trimmed one clip at a time against the measured pair (that
clip's R5 margin, and that clip's own worst 60 Hz angular step). The target was
a readable margin — 70–180 mm where the mechanism gives a choice — and no deeper,
because depth a clip does not need is continuity headroom spent for nothing.

**19 of 19 hosting clips reach B strictly lowest. Worst margin +37 mm (slot 9,
against a 20 mm gate). G9 worst 7.642° against the UNMOVED 8.0° ceiling.**
Measured with `CONFIG JUDGED (mrear): every shipping constant at its shipped
value` printed on the same run.

| slot | share pm | B-lowest margin | returns to | | slot | share pm | B-lowest margin | returns to |
|---|---|---|---|---|---|---|---|---|
| 0 | 715 | +171 | +444 | | 11 | 815 | +105 | +422 |
| 1 | 730 | +99 | +401 | | 12 | 650 | +157 | +434 |
| 2 | 680 | +188 | +468 | | 14 | 635 | +123 | +363 |
| 3 | 730 | +137 | +435 | | 17 | 635 | +132 | +377 |
| 4 | 650 | +144 | +456 | | 18 | 590 | +112 | +407 |
| 5 | 770 | +150 | +436 | | 19 | 645 | +131 | +417 |
| 6 | 730 | +161 | +414 | | 20 | 815 | +67 | +452 |
| 8 | 770 | +83 | +431 | | 22 | 800 | +70 | +447 |
| 9 | 950 | +37 | +400 | | 23 | 715 | +171 | +444 |
| 10 | 820 | +80 | +401 | | | | | |

⚠ **Slot 23 caught a third report naming the wrong operand.** It is the
fixed-camera bake of the idle and every SCHEDULED layer is called with
`kIdleOrbitSlot`, so it renders on slot 0's share (715) while its own
`slot_id` is 23 — past the end of the table. Three lookups indexed on the clip's
`slot_id` and fell onto a 750 default the renderer never used: R5's printed
share, R5's "does this clip author a dip" test, and `swallow_nodules`' own
fallback. `u02::knead_schedule_slot()` names the mapping once and all three use
it. Production is unaffected — the shipping Hover / Inspect / Taunt III / Blown
sequence CRCs are byte-identical across the change (`0x93B95AEE`, `0xD00478A1`,
`0xC81598AA`, `0xC6AAF7AD`) — because production already passed slot 0. The
fallback was a silent default waiting for the next clip whose slot runs past the
table, which is the pass-5 orphaned-index fault exactly.

**The clips that do NOT host the beat, and why — every one named:**

| slot | what it is | why it does not host a dip |
|---|---|---|
| 7 | Still | a two-frame form diagnostic; there is no time for a gesture |
| 13 | Trick | the plant contact is pinned; a carrier beat under it would fight the contact |
| 21 | Taunt III | its own crown shuffle already owns the carrier rankings — and it reads **+64 mm**, i.e. B *is* the lowest ball there, by the mechanism the owner named as the reference |
| **15** | the mana lab (lane-only, Direction 6) | **it never calls `antenna_knead`/`swallow_nodules`.** It runs a forked `lab_antenna_knead` on its own timeline, so the dent could not reach it however the table were set |
| **16** | nodule-solo | same: `build_nodule_solo` does not call the knead layer, and the clip exists to show each nodule moving **independently** — a dip pressing B would be the two-authorities fault against the thing it demonstrates |

15 and 16 carried **750** before this close, so R5 counted them as hosting and
reported them permanently short (−191 and −74 mm, flat at every depth). That is a
gate leg that **cannot reach its state** being read as a finding. They are now
**0**, the honest declaration, and R5 judges 19 clips instead of 21.

So: of the 24 clips in the bank, **20 have B strictly the lowest ball** (19 by
the dent, Taunt III by its crown shuffle), and the four that do not are two
diagnostics that cannot run the layer, one two-frame form plate, and the pinned
Trick plant.

## 4. The particle reaction, re-authored so it READS

The first version added `dip_pm` to `agit`, the agitation scalar the fold already
runs near the top of. Measured against the reaction switched off, on Hover, in
production ink: **588 of 600 frames byte-identical**, 72 px of 92,160 past
24/255 at the strongest, against 6,145 for the geometry beside it. At 10× it was
one lightning bolt a few pixels longer. CLAUDE.md's crayon grain, exactly — and
the identity leg ("`FOLD_DIP_PM=0` changes the bytes") was **true** and said
nothing about visibility.

It is now a **motion of the whole mana body**, in the owner's own Direction 7
vocabulary ("the shapes should look a bit malleable like they're being knead").
When B presses down, the lightning figure and its particle cloud — including the
wanderers, which are the mana that gets pushed *out* of the pocket — flatten,
spread sideways and ride down, about the **same pivot** and through the **same
separable law**, and come back with the press:

* `kFoldDipDropMm = 330`, `kFoldDipSquashPm = 430`, `kFoldDipSpreadPm = 700`,
  all per mille of the full reaction and all scaled by `dip_pm`, so
  `ZHAO_U02_FOLD_DIP_PM` still ladders the whole thing and **0 is still the
  exact-off control** (`dip_squeeze` returns before a single arithmetic
  operation). Population, palette and the distance-scaled line law are untouched.

**How it was judged.** By eye, at native and 3×, on Inspect, Hover and Drift,
against the exact-off bank at the same frames, off the ladder
`ZHAO_U02_FOLD_DIP_PM ∈ {0, 300, 650, 1000}`. At 0→300 the bolt figure barely
moves; at 650 the figure and the mote cloud visibly drop and spread while the
press is on and are identical to the control outside the window; at 1000 it
starts to read as the mana sliding rather than being squeezed. **650 ships.**
Drift is far enough away that the reaction is properly small there and the thin
distance-scaled lines are unchanged.

The measurement is on the comparison side only, and it was **re-taken from the
shipping build after the constants were raised** — quoting the ladder rung the
eye rejected would be this pass's own defect:

| subject | frames not byte-identical to the reaction off | strongest frame, px past 24/255 |
|---|---|---|
| Hover (600) | **127** (was 12) | **2,745** (was 72) |
| Inspect (600) | 127 | **3,417** |
| Drift (300) | 40 | 283 — correctly small at that distance |

Those numbers are not the acceptance — the pictures are — but they are the bias
check the earlier version failed.

## 5. The three gate items

### 5a. G10 DENT PIN — built, and its control caught it being blind

Specified in `P20-SOLVER-ARCHITECTURE.md` revision 1 and never built; the dent's
central contract ("A and C keep their world position AND frame, which is why the
attachment is charged nothing") existed only behind a private
`-DZHAO_P20_PINPROBE` rebuild. The probe is now a runtime-gated accumulator the
gate switches on before the bank is built, and the leg bounds it.

**Its first version measured POSITION only, and under its own control it read an
unchanged 8 mm** while the closure and continuity legs went red downstream. The
reason is structural: `loop_walk(g, 4, …)` composes HingeC's rotation into `Q`
*after* it has advanced the position, so C's **position does not depend on the
pin at all**. That is CLAUDE.md's "detector wired to two operands that move
together" — the quantity being differenced was blind to the fault the leg was
built for, and it would have shipped reading a reassuring number.

It now measures the **frame** as well:

| | position (L1) | frame drift |
|---|---|---|
| shipping | 8 mm (ceiling 20) | **0.066°** (ceiling 4.0) |
| `--fail-dent-pin` | 8 mm | **93.658°** |

attributed `0x10300`. The allowed category set is wider than the expected one on
purpose, and the reason *is* the leg's argument: dropping the pin breaks G10 and
the two things the pin exists to protect. A control that fired G10 alone would
have to be a perturbation rather than the real defect.

### 5b. R5's `dip_stuck` arm — a control, and the same law again

`--fail-dip-stuck` freezes `knead_dip_window_env` at a full envelope: the dip
goes down on the first key and never returns.

Building it exposed the arm as blind too. It differenced **B's own vertical
travel** over the clip and asked for 60 mm — a quantity dominated by every other
layer (the ambient nodule schedule, the breathing, the body's own rise and fall),
so a dip frozen permanently at the bottom still measured **280–520 mm** and the
control returned **rc 0**.

The dip's contract is a *ranking* that goes and returns, so the return is now
measured on the ranking: somewhere in the clip B must be at least 60 mm **above**
the lower of A and C again. Nothing but the dip can hide that. Shipping clips
return to **+363…+468 mm**; the control fires mask `0x10`. The per-slot R5 line
prints the margin on every clip, passing or not, so the leg can be read for
margin rather than only for failure.

### 5c. The hand-off — governance restored, not just reported

Row 3 of the audit: the bound went 320 mm hard → reported-only, and shipping read
**361 mm** — past a number the gate was still printing, with nothing guarding it.

The demotion's *reasoning* was right about the old number (320 was calibrated
against a band that could not bend), and wrong to conclude "therefore no bound".
So the bound is **re-calibrated for the new solve rather than removed**:
`kGateHandoffMaxMm = 420`, 16% over the shipping 361 — and the shipping 361 is
identical with `ZHAO_U02_KNEAD_DIP_PM=0`, so the beat costs it nothing. It is a
regression ceiling and claims nothing about what the right curvature is.

**Fired:** `--fail-rear-frame` (the version-18 legacy-root End frame — the defect
this pass repaired) drives it to **523 mm**, and R4 now prints which of its four
conditions breached: `[rail-floor rail-ceiling hand-off ]`. `--fail-rear-joint`
reads 393 and deliberately stays under, so it keeps failing only its own R1
detector and the matrix masks are unchanged.

## 6. The override audit

Swept every gate and driver (`manafold_{spangate,rear_audit,nodule,motiongate,
probe,outlinegate,shellgate,meshcheck,public_jointgate,qa_p12,eyesize}.cpp`) for
assignments to a `g_u02_*` shipping global. Every remaining one is:

* a **declared mutant** behind a `--fail-*` flag or `break_check`, saved and
  restored around the leg, and printed in a `[MUTANT]` banner; or
* **strict environment parsing** (a malformed value returns RC 2); or
* `manafold_shellgate.cpp`'s `regression_control()`, which sets the pass-15
  shell wholesale and is an explicitly **inverted-polarity** control that says so
  in its own banner.

**No second `--dip` was found.** One weakness is recorded rather than fixed:
`manafold_rear_audit.cpp` parses six of its authoring-ladder environment
variables with bare `std::atoi`, so a malformed value silently becomes 0 instead
of returning RC 2 the way the reel's own parser does. The banner below makes the
*consequence* visible even so.

**The durable fix is the banner.** `u02::print_judged_config(who)`
(`manafold_art.h`) prints, in one line, the value each override-reachable
constant is **actually being judged at**, beside its shipping value, and marks
any that differ with `**OVERRIDE, shipping N**`. mspan and mrear both print it
before their first figure. A deliberate override is now named in the log by
construction, and a headline taken from an overridden run cannot be mistaken for
a shipping one by a reader, a later session or a matrix. The rule the rows
encode: *a gate reads the shipping constants; it does not choose them.*

## 7. Identity, and what moved

**The pass-19 contract holds exactly.** `ZHAO_U02_KNEAD_DIP_PM=0
ZHAO_U02_REAR_BOW=legacy` reproduces `P19-FINAL-BANK-INTEGRITY.md`'s own recorded
sequence CRCs: Hover `0xA2D0E051`, Inspect `0x779615BB`, Taunt III `0x75BC4777`.
That leg is now in the matrix (`e-identity-pass19`), where it was not before.

Three identity legs are re-baselined, and the reason is declared rather than
absorbed: the dip's **shared schedule** (the ramp floor and the per-clip shares)
feeds the carried solver as well as the dent, by design, so a legitimate change
to the beat moves the carried bank too. The carried solver is a *selectable
alternative mechanism*, not a legacy contract; the legacy contract is the leg
above, and it did not move.

| leg | configuration | CRCs (hover / inspect / taunt3) |
|---|---|---|
| `e-identity-pass19` | `KNEAD_DIP_PM=0 REAR_BOW=legacy` | `0xA2D0E051 0x779615BB 0x75BC4777` — **unchanged, = pass 19** |
| `e-identity-dipoff` | `KNEAD_DIP_PM=0` | `0xEFE5AFC1 0x9E71DF79 0xC81598AA` (new leg) |
| `e-identity-carried` | `KNEAD_DIP_SOLVER=carried` | `0x40AA70E8 0xF6BD3444 0xC81598AA` (re-baselined) |
| `e-identity-legacy` | `carried` + `REAR_BOW=legacy` | `0x46A9C936 0x99CCAC21 0x75BC4777` (re-baselined) |

## 8. Bounds

**No bound was relaxed.** `kAntennaMaxAngularStepDeg` (8.0), the accel and jerk
ceilings, `kSpanStretchMaxPm`, `kSpanCompactionMinPm`, `kSpanMinRunMm`, the ±2 pm
bound tolerance, `kGateDipMarginMm` (20) and the R4 rail floors and ceiling are
byte-identical to the branch point. Two bounds were **added** (`kDentPinMaxMm`,
`kDentPinMaxFrameA16`) and one was **raised from a demoted state back into
force** (`kGateHandoffMaxMm`, 320 reported-only → 420 enforced), which is the
review's follow-up 5.

## 9. Files changed at the close

* `tools/reel/manafold_clips.h` — `arc_from_y_about`; the world axis on
  `nodule_aim_rollstable`; the fold-plane normal carried into both dent aims; the
  always-available G10 pin accumulator (position **and** frame) and its control;
  the `dip_stuck` control in `knead_dip_window_env`; the ramp-floor global and
  `ZHAO_U02_KNEAD_DIP_RAMP_KEYS`.
* `tools/reel/manafold_art.h` — `kKneadDentDepthPm` 4200; `kKneadDipMinRampKeys`
  30 and its global; the re-trimmed `kKneadDipClipPm`;
  `kFoldDipDropMm/SquashPm/SpreadPm`; `kDentPinMaxMm`, `kDentPinMaxFrameA16` and
  the pin globals; `g_u02_knead_dip_stuck_control`; `print_judged_config()`.
* `tools/reel/manafold_fx.h` — the dip squeeze (`dip_ax` / `dip_squeeze`) applied
  to the lightning figure in `place()`, to the independent mote lane and to the
  wanderers.
* `tools/reel/manafold_spangate.cpp` — G10 DENT PIN, `--fail-dent-pin`,
  `kCatDentPin`, the judged-configuration banner.
* `tools/reel/manafold_rear_audit.cpp` — `--fail-dip-stuck`; the ranking-based
  return measurement; the per-slot R5 margin line; `kGateHandoffMaxMm` 420 back
  in force with per-condition attribution; the judged-configuration banner.
* `P20-RECEIPTS/gatematrix_p20.sh` — `s-dent-pin`, `r-dip-stuck`, four new
  selectors, `e-identity-pass19`, `e-identity-dipoff`, two re-baselines.

## 10. The attachment parity, at 1.9x the previous depth

The dent's central claim is that F–A and C–E have **no term in the solve** — not
a tolerance, an absence — so the attachment cannot be charged for the gesture.
At depth 4200 with a 30-key ramp, every R1 and R4 number is **identical to the
dip switched off, digit for digit**:

| | shipping (dip on) | `ZHAO_U02_KNEAD_DIP_PM=0` |
|---|---|---|
| worst arm↔End rotation | 16.48° | 16.48° |
| worst rear centreline turn | 113.03° | 113.03° |
| worst rear rail | 0.692 .. 2.052 | 0.692 .. 2.052 |
| worst hand-off | 361 mm | 361 mm |
| worst rail step | 0.1630 | 0.1630 |

`P20-RECEIPTS/close-attachment-parity.txt`. This is also the strongest
corroboration of G10 DENT PIN's 0.066° of frame drift at C: **if the pin leaked,
these rows could not match.** The gate and the parity are independent
instruments agreeing, which is the only kind of agreement worth quoting.

It also retires the last doubt about item 1. The pass exists because the rear
attachment tore; the beat that was supposed to threaten it most, at nearly twice
the depth the review measured, moves it by nothing at all.

## 10a. The review's five follow-ups, answered

| # | the review asked for | what happened |
|---|---|---|
| 1 | *"Decide item 2 with the owner's eye, not with a knob. The dent cannot reach 'B lowest on every animation' inside G9. The path named by the pass's own earlier packets — redistribute across A/B/C as Taunt III's crown shuffle does — is still the right one, and it is authoring."* | **Answered differently, and the redistribution was not needed.** The premise was the pass's own conclusion, and it was wrong: the dent *can* reach it inside G9, because the trade the ladder showed was a property of the AIM's axis quantisation, not of the mechanism. 19 of 19 hosting clips, G9 at 7.642. The eye still chose the depth — off a rendered ladder — but it was choosing inside a range the repair opened, not between two things that could not both exist. |
| 2 | *"Item 3 needs a visible amplitude. `kFoldDipOnsetMm` = 90 against a sag that rarely clears it. Lower the onset, or drive the reaction from the dip's schedule rather than from the achieved sag, then look at it."* | Done, and by a third route the review did not name. The onset was **not** lowered and the reaction still reads from the POSE rather than the schedule — both of those were right and neither was the problem. The problem was that the reaction had no visible CHANNEL: it only added to a scalar the fold already ran near the top of. It is now a motion of the whole mana body. §4. |
| 3 | *"Build G10 DENT PIN with `--fail-dent-pin`. The probe already exists and the shipping number is 7 mm; a leg bounding it would cost little."* | Done — and the leg's first version was blind, which the control caught. §5a. |
| 4 | *"Give R5's `dip_stuck` arm a control, or fold the arm into the one that has one."* | Given a control, and the arm turned out to be measuring an operand no control could move. Both repaired. §5b. |
| 5 | *"Put a regression ceiling back on the hand-off — reported **and** bounded, at a number above the shipping 361 with margin."* | Done: 420 mm, enforced, fired at 523 by an existing causal control. §5c. |

Two of the five were answered by disagreeing with the review's diagnosis while
agreeing with its finding, which is the useful kind of disagreement: in both
cases the review was right that the thing was broken and right about how to tell.

## 11. Evidence index

| picture | what it decides |
|---|---|
| `P20-LOOKS/CLOSE-DEPTH-LADDER-JUDGING-3X.jpg` | the depth ladder {OFF, 2800, 3400, 3800, 4200} on the fixed antenna judging view — a smooth progression from pointed arch to sagged bar, tube throughout, no slab and no kink |
| `P20-LOOKS/CLOSE-DEPTH-LADDER-TOPRUN-4X.jpg` | the same ladder on the top run alone, where the ranking reads |
| `P20-LOOKS/CLOSE-KNEAD-OFF-VS-SHIP-5X.jpg` | dip off vs shipping at three frames of the beat |
| `P20-LOOKS/CLOSE-KNEAD-NATIVE.jpg` | **the shipping read, at native resolution**: the press, the recovery, the resting loop, and the off control |
| `P20-LOOKS/CLOSE-JUDGING-ALLFRAMES-SHEET.jpg` | all 420 frames of the judging view — breakage check |
| `P20-LOOKS/CLOSE-HOVER-ALLFRAMES-SHEET.jpg` | all 600 frames of Hover at the shipping configuration — breakage check |
| `P20-LOOKS/CLOSE-PARTICLE-BEFORE-REAUTHOR-4X.jpg` | what the reaction looked like before: one bolt a few pixels longer |
| `P20-LOOKS/CLOSE-PARTICLE-REACTION-AB-3X.jpg` | **the shipping reaction**, off vs on, on Hover f269 and Inspect f268 — taken from the shipping build |
| `P20-LOOKS/CLOSE-PARTICLE-REACTION-WINDOW-2X.jpg` | the reaction across the whole dip window, off vs on, including f340 outside it where the two are identical |
| `P20-LOOKS/CLOSE-PARTICLE-INSPECT-DRIFT-3X.jpg` | the reaction on Inspect, and on Drift where the distance-scaled lines are thin |

| receipt | what it holds |
|---|---|
| `P20-RECEIPTS/gate-matrix-close.txt` | the full matrix, one invocation, the beat ON |
| `P20-RECEIPTS/close-gain-ladder-newaim.txt` | the gain and depth ladders with the repaired aim, against the review's own |
| `P20-RECEIPTS/close-override-audit.txt` | the audit, and the banner's own positive control |
| `P20-RECEIPTS/close-attachment-parity.txt` | §10 |
| `P20-RECEIPTS/close-mrear-normal.txt`, `close-mspan-normal.txt` | the shipping runs, banner included |
| `P20-RECEIPTS/close-control-dent-pin.txt`, `close-control-dip-stuck.txt`, `close-control-handoff.txt` | the three new controls, fired |
| `P20-RECEIPTS/close-binaries-md5.txt` | the build the receipts came from |
