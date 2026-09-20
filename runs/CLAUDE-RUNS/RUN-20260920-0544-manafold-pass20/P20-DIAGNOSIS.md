# Manafold pass 20: diagnosis

**Date:** 2026-09-20
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-21-2026-09-20.md`, plus
the owner's mid-run correction to item 1 (below).
**Worker:** Claude (sole Opus worker; no Qwen, no sub-agents)
**Baseline binaries:** `.tmp/p20-base/bin`, built direct from `manafold-pass20`
at `91f8fbec` with `tools/reel/build-direct.sh`.

---

## 0. The correction that re-scoped item 1

Direction 21 as written said the rear connecting part is *pulled out of the
body*. Mid-run the owner corrected it, verbatim:

> "the rear doesn't leave the body, but it rips a big piece out and it stretches
> too much, which leads me to conclude there's too much motion in the back
> nodule."

So item 1 is **not** an emergence/burial breach. Nothing here chases a burial
threshold. The fault is a **surface** fault: the skin at the return is stretched
and folded until a broad flat flap stands proud of the body.

---

## 1. What is actually wrong (item 1)

### 1.1 The read, by eye

`manafold-inspect`, production ink, 601 frames. Frames ranked by **badness**
(worst rear strain), not by index. The worst cluster at keys 77–80 and 188–191.

At 4x on frames 380 and 316, and on a 12-tile strip across f356–f400, the return
leg's lower section **alternately splays into a broad flat wedge and narrows back
into a rod**. Through f368–f384 it is a wide triangular sheet lying over the
body with a stepped silhouette and a hard black crease running into it. That
sheet is the "big piece ripped out". It appears and disappears; it is an event,
not authored shape.

### 1.2 The measurement, on the comparison side

New in `manafold-rear-audit`: **posed skin strain**, per (ring, segment), against
the same edge in the **rest pose** (slot 7, this codebase's own static form
diagnostic).

| clip | rear rail max | rear rail **min** | at |
|---|---|---|---|
| Inspect/Hover (0) | 1.249 | **0.147** | ring 49 |
| Rest (5) | 1.055 | 0.212 | ring 49 |
| Taunt III (21) | 1.441 | 0.328 | ring 49 |
| Channel (2) | 1.103 | 0.129 | ring 49 |
| Drift (1) | 1.026 | 0.184 | ring 49 |
| Trick (13) | 1.349 | 0.737 | ring 49 |
| **Still (7)** | **1.000** | **1.000** | — |

A longitudinal skin edge at ring 49 is driven to **14.7% of its rest length**.
That is not a stretch, it is a fold: the rings pile through one another and the
surface between them splays outward. 1.25–1.44 the other way is the stretch.
Standing still, every one of these is exactly 1.000.

### 1.3 The cause, named

The instrument reports two more things, and the second is the answer.

**Two-bone hand-off disagreement** — for each two-bone vertex, the distance
between where bone b0 alone would put it and where b1 alone would put it. LBS
places the vertex on the straight line between those points, so this length is
the size of the fold the blend can produce before any weight is chosen:

| clip | worst disagreement | where |
|---|---|---|
| Inspect | **260 mm** | ring 40, bones 20/21 |
| Rest | 244 mm | ring 40, bones 20/21 |
| Taunt III | 203 mm | ring 40, bones 20/21 |
| **Still (7)** | **9 mm** | ring 40, bones 20/21 |

Bones 20/21 are `kBSpanDeltaEStart` / `kBSpanDeltaEMid` — the **rear signed-span
helpers**. So the motion straining the skin is carried by the span, not by the
socket blend.

**Rear span excursion** — `kBSpanDeltaE` carries, as an unskinned receipt, the
full `|C -> socket|` distance minus its rest value: the amount the rear span is
asked to stretch on that sample. `kRearSocketFromCMm` is **1010 mm**.

| clip | excursion |
|---|---|
| Inspect/Hover | **+160 … −662 mm** |
| Rest | +13 … −620 mm |
| Taunt III | +272 … −517 mm |
| Trick | +241 … −224 mm |
| **Still (7)** | **+0 … −24 mm** |

**The rear span is telescoped by up to 662 mm — 66% of its own length — twice
every Inspect loop, against 24 mm standing still.** The whole excursion is
written into three helper bones as pure +Y translation, so the rear skin absorbs
all of it. A span driven to 34% of its length is exactly a rail at 0.147.

**That is the defect.** "It stretches too much" is literally true and the number
is 66%.

### 1.4 The owner's inference, corrected honestly

The owner concluded "too much motion in the back nodule". Switching the back
nodule's ambient rotation **entirely off** moves the fold from 0.147 to **0.150**
— nothing. The End carrier's own rotation is not the magnitude driver.

What it *does* drive is the **rate**: at ambient gain 1000 the per-sample strain
step goes 0.0296 → **0.0983**. The back nodule's oscillators are what make the
flap flick, which is why it reads as "too much motion" — but damping them alone
cannot remove the flap. Both halves need attention: bound the span (the flap)
and keep the ambient calm (the flick).

### 1.5 Why pass 19's gate passed 128/128 with this visible

Not an oversight — a structural blindness, and it is the CLAUDE.md
"detector wired to operands that move together" law in a new costume.

* **`manafold-rear-audit` averages each ring's 8 vertices into one centroid
  (`cen[]`) before it measures anything.** `bend`, the End/last/C path,
  velocity, acceleration and jerk summary are all built from those centroids. A
  ring pair collapsed together still has two valid centroids in the right places
  along the centreline. `rel`, `axis` and `sock` are bone rotations, which the
  ablation above shows are not where the fault lives. **Every metric in the
  rear gate is computed from a quantity in which this defect cancels.**
* **`mspan`** checks station bookkeeping, zone identity, extension, compaction
  and closure — arc-length accounting, not posed surface strain.
* **`mprobe`** checks clearance, closure and burial — where the surface is, not
  how much it is being stretched.
* **`mmeshcheck`** checks the **bind** mesh. It never sees a posed frame.
* **`mjointpub`** enforces a **20 mm End-motion FLOOR**. It asks for *more* rear
  motion; it can never object to too much.
* **`msmooth`** checks effect/particle identities and 60 Hz continuity.

**No gate in the 128-leg matrix measured a posed surface at all.** The matrix
was complete with respect to every question anyone had asked it, and silent on
the one the owner was looking at. A green matrix was then quoted as evidence
about a surface no leg of it touches.

---

## 2. Item 2: the mechanism to generalise

The owner named the reference: *"nodule taunt already does the kneading motion at
times."* That is slot 21 / Taunt III, and inside it the relevant mechanism is
Direction 16's **CROWN SHUFFLE**, whose own comment states the intent exactly:

> "56..132 THE CROWN SHUFFLE. Four held A/B/C rankings give every free carrier
> top and bottom ownership."

Already in the tree, already authored, already C2:

* `kTaunt3OrderRank[4][3]` (`manafold_art.h:3533`) — a rank per carrier per
  tableau: `+1` high, `0` mid, `-1` low.
* `kTaunt3OrderHighMm / MidMm / LowMm[3]` — the height each rank means, per
  carrier. **B's low is −325 mm, the deepest of the three** (A −175, C −150).
* `taunt3_order_target / _blend / _pose` (`manafold_clips.h:1880–1950`) — monotone
  `motion_c2_ease` attacks, exact holds, a twelve-key release to zero.
* `swallow_nodules(g, swal[5], lean_pm)` (`manafold_clips.h:1824`) — the **one
  production consumption point** for all five carriers (front, A, B, C, End),
  under the shared public mute and attachment law.
* `antenna_knead(...)` (`manafold_clips.h:2368`) — **already the always-on layer
  every performing clip calls**, with a per-slot gain table `kKneadClipPm`.

So item 2 is a generalisation, not a second system:

* **Shared:** the rank→height authoring shape, `motion_c2_ease`, the
  `swallow_nodules` consumption point, the `antenna_knead` host layer, and the
  per-slot gain discipline.
* **Per-clip:** when the dip happens and how long it takes — every clip has a
  different length, and one-shots have no loop seam to honour while loops do.

The dip's requirement — *B becomes the LOWEST carrier* — is a **ranking**
statement, which is precisely what the crown shuffle already expresses. It is
not a new displacement channel.

---

## 3. Item 3: the particle reaction

The mana fold population is driven from `u02::fx_anchors_from_pose(T, pose)` and
consumed by `mana_fill(3, ...)` / `mana_lightning(...)`, with pass-19's
distance-scaled line law (`ManaSplat::line`, `mana_line_r_px`,
`kManaLineFullRadiusPx = 360`). The plan is to give the dip a named scalar the
fold population can read, so the particles gather/agitate on the knead rather
than ignoring it, with the v18/v19 mana character and the distance-scaled lines
untouched, and an exact-off control.

---

## 4. Plan

1. **Bound the rear span excursion** with a named, C1, monotone soft limiter, so
   the rear return can only telescope within an authored range. Ladder it by
   eye. Keep the socket's own absolute attachment untouched, so nothing detaches.
2. Keep the End ambient calm (pass 19's 400) so the *rate* stays down.
3. **Gate R4 STRAIN (mask 0x8)** in `manafold-rear-audit`: rear rail strain and
   hand-off disagreement bounded on every key and midpoint of every clip, with a
   committed positive control that restores the pass-19 unbounded span and must
   fire. The gate asserts the *correct* behaviour; the control is separate.
4. Generalise the crown shuffle into an always-on authored **B dip**.
5. Give the dip a particle read.

**Risk to watch:** `mspan` owns the signed-span contract. Bounding the span
excursion changes exactly what it measures, so its normal legs are the first
thing to re-run after the limiter lands.
