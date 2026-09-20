# The kneading dip: option (a) built, and the stop condition

**Date:** 2026-09-20 (packet 4)
**Instruction:** take option (a) — a dedicated vertical on B that does not
lengthen `C→socket`. Do not widen `kSpanStretchMaxPm[C-E]`. If B still cannot
become strictly lowest without touching an attachment bound, **stop and say so
with the ledger**.

**This is that stop.** The mechanism was built and it works as designed. It is
not enough, and the ledger below names exactly what would have to give.

---

## 1. The mechanism, plainly

`loop_pose`'s nodule solve is a **sequential carried solve**: each carrier's
target is its *carried* position plus its offset — "all the nodules should be
able to move individually **and bring the antennae parts with them**". So
lowering B also lowers C, because C hangs off B. C is where the return arm
starts, so C's descent swings `|C→socket|` and the **C-E span pays for the whole
gesture**.

That is why packet 3's seven rankings all landed on the same C-E figure: the
breach was never about *which* carriers moved, it was **the carry**.

**The dedicated vertical (`kKneadDipCarryCancelPm`):** B goes down by d, and C
is handed +d back, so C's net world motion is ~zero and the rear run does not
see the beat. The reaction is absorbed inside the A..C stretch, because
`nodule_aim` stretches each span to reach its target — so the A→B and B→C spans
lengthen and shorten around a **stationary C**. Those are the existing
signed-span helpers `kBSpanDeltaB` and `kBSpanDeltaC`. No new bone, no new
channel.

**It demonstrably works.** With the carry cancelled, C-E becomes responsive to
amplitude again instead of pinned, and the ranking improves sharply:

| cancel | gain | C-E stretch | B strictly lowest |
|---|---|---|---|
| off | 550 | +408 mm | **0 / 21** |
| **on** | 550 | +408 mm | **9 / 21** |
| on | 650 | +442 mm | **13 / 21** |
| on | 800 | +486 mm | 16 / 21 |
| on | 1000 | +529 mm | 17 / 21 |

## 2. The span ledger, per station

Centre lengths: F-A 680, A-B 340, B-C 380, C-E 1010 mm. Bounds are per mille.

**Before (no dip):**

| span | mm | pm | stretch bound | compaction bound | verdict |
|---|---|---|---|---|---|
| F-A | −198 … +199 | −291 … +293 | 320 | −320 | ok |
| A-B | −107 … +164 | −315 … +482 | 480 | −330 | ok at rest |
| B-C | −137 … +145 | −361 … +382 | 400 | −430 | ok |
| **C-E** | −687 … **+305** | −680 … **+302** | **440** | −700 | ok |

**Shipping (dip on, gain 550, carry cancel OFF) — fully green:**

| span | mm | pm | verdict |
|---|---|---|---|
| F-A | −198 … +199 | −291 … +293 | ok |
| A-B | −107 … +164 | −315 … +482 | ok |
| B-C | −137 … +145 | −361 … +382 | ok |
| **C-E** | −687 … **+408** | −680 … **+404** | ok, 36 pm of headroom |

**With the carry cancel at gain 650 (13/21 strictly lowest):**

| span | mm | pm | verdict |
|---|---|---|---|
| F-A | −198 … +199 | −291 … +293 | ok |
| A-B | −107 … +164 | −315 … **+482** | **breach, +2 pm** |
| B-C | **−168** … +145 | **−442** … +382 | **breach, −12 pm** |
| **C-E** | −687 … +442 | −680 … **+438** | **ok — 2 pm inside** |

**Read that last row.** With the dedicated vertical in place, at the amplitude
that reaches 13 of 21 clips, **the attachment span is inside its bound.** The
mechanism did what it was asked to do: the residual moved off C-E and onto two
interior spans between the free carriers.

## 3. Why it still stops

Letting those two interior spans give (A-B +480→+490, B-C −430→−450, both away
from the attachment, with `kSpanMinRunMm` untouched and the remaining run at
212 mm against its 80 mm floor) **clears all 6 bound breaches** — and then two
*other* mspan legs fail instead:

* *a shipping visible carrier has an angular step/acceleration/jerk discontinuity*
* *SpanDeltaE and body-attached RearSocket do not meet at End*

Those persist with the carry cancel at **every** amplitude tried, down to gain
350 (where the dip reaches 0/21 and is invisible). They are continuity and
closure, not envelope — the cancel buys attachment headroom by making C hold
still while the spans either side work harder, and past a point the solve cannot
keep the chain smooth or land the closure.

## 4. What has to give, by how much, and the owner's choice

To reach **all 21** clips, B must travel roughly as at gain 1000, where
**C-E reads +524 pm against its +440 ceiling — an 84 pm (19%) overshoot of the
attachment bound.** The carry cancel reduces but does not remove that.

So the choice is one of:

1. **Accept a partial read.** Ship at gain 650 with the two interior bounds
   given ~2 and ~12 pm — **13 of 21 clips** show B strictly lowest, the
   attachment bound is untouched and 2 pm inside — *and* accept the two mspan
   continuity/closure legs going red, which is not acceptable as it stands.
2. **Rework the nodule solve** so B's vertical does not stretch the A..C spans
   at all — a real second channel on B rather than a target offset the spans
   must reach. That is a solver change, not an authoring one, and it is the only
   route that gets all 21 without touching a bound.
3. **Revisit `kSpanStretchMaxPm[C-E]`** — refused, and I have not touched it.

**Shipping is option 0: gain 550, carry cancel OFF, every gate green, and B does
NOT reach strictly lowest on any clip.** The knead reads — the loop's top-middle
presses down and the loop squeezes — but the owner asked for *lowest*, and this
is not that. **It is not being reported as done.**

## 5. What is committed

`kKneadDipCarryCancelPm` ships at **0** with the mechanism intact and this
ladder in its comment, so the next packet inherits a working dedicated vertical
and the evidence, not the argument. `kSpanStretchMaxPm` / `kSpanCompactionMinPm`
are exactly as pass 19 left them.
