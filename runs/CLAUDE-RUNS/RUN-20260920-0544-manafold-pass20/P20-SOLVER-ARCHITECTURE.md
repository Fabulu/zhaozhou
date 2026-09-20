# P20 solver architecture: THE DENT — a pinned re-fold of the A–B–C triangle

**Revision 2** (2026-09-20, after `P20-DENT-EXPERIMENT.md`, implementation
`b7c096c2`) is §R2 below. **Revision 1** follows it unchanged as history; where
the two disagree, R2 governs.

---

## R2. Revision 2 — the walk was wrong, the swing is free

### R2.0 Verdict

Both failed criteria were measured through **one implementation defect** in
the dent's own chain walk, at exactly the moment that matters. The pin is
recoverable **by construction**. The crossing compaction is real physics of a
*planar* press — and it disappears entirely if B goes to its mirror by a
**rigid rotation about the A–C chord** instead of through it. That is the
coordinator's "partially rotational dent", taken all the way: zero span-length
change on every sample, F-A and C-E untouched, A and C pinned, and the same
final pose R5 already accepts. Depth beyond the mirror, where some clips need
it, is a named **overpress** that stretches only the two interior spans while
the ambient is fully ducked, inside their existing bounds. No bound changes.
No owner approval is needed for the mechanism — only the eye's verdict on the
out-of-plane read, and R2.5 names the fallback if the eye refuses it.

### R2.1 (a) Why the pin leaked — and that it did not

The closure walk pairs `kLoopArcMm[i]` with `span_child[i] = {kBNeck,
kBHingeA, kBHingeB, kBHingeC, kBHingeD}`: **a span's delta lives on the bone
that ENDS it.** The 680 mm F→A span reads `local_t[kBHingeA][1]`, which is
precisely what `set_span_delta(0, …)` writes (`kChild[0] = kBHingeA`);
`finalize_rear_follow` uses the same pairing, and G7 (10 mm) has validated it
for the whole bank.

The dent's walk (`manafold_clips.h:835–842`, commit `8fbd7e97`) reads
`len_of(kBNeck, kLoopArcMm[1])`, `len_of(kBHingeA, kLoopArcMm[2])`,
`len_of(kBHingeB, kLoopArcMm[3])` — **one bone early on all three spans.**
`local_t[kBNeck][1]` is always 0, so the walk applies no delta to F→A, the F-A
delta to A→B, and the A-B delta to B→C. Its A, B and C are therefore wrong by
up to the ambient deltas (F-A alone reaches ±199 mm).

The consequences match the experiment exactly:

* **Duck 1000, full envelope:** every delta is zero, both walks agree, and the
  pin held — C-E at or below no-dip. That row *is* the pin working.
* **Duck 0:** the second aim chases a wrong "saved C"; the real C lands
  displaced by the walk's error. That is the +19 / −11 mm.
* **The crossing lives mid-ramp** (s passes 1000 at env ≈ 0.5), where the duck
  is only half on, so the walk was wrong on precisely the samples criterion 3
  measured. The aims' `want − len` from misplaced points ships as compaction.
  The 3× is at least partly this; how much is real is unknown until the walk
  is fixed, and **no number from `P20-DENT-EXPERIMENT.md` §3 should be quoted
  again** until it is re-measured.

**Recoverable by construction: yes.** After the fix the pin's residual is
angle16 quantisation in the two aims (≈ 0.05 mm per stage; ≈ 1 mm worst case
in `asin16`'s ill-conditioned region), nothing that scales with the gesture.
The criterion stays **≤ 1 mm on C-E, per sample, duck OFF.**

**The fix is one shared walk, not a corrected copy.** Revision 1 declared "no
refactor of the closure walk" as an omission; that omission cost exactly this
— two walks drifted at birth. Factor `loop_walk(const Rig&, int spans, P&, Q&)`
out of the closure walk *verbatim* (same expression, same `>> 16`), call it
from the closure and from the dent, and let the existing 4-subject CRC identity
leg prove the closure did not move a byte. `finalize_rear_follow` keeps its own
copy this packet (it walks clip tracks, not a `Rig`); its pairing is the
reference the helper is checked against.

### R2.2 (b) Where the crossing length goes: nowhere — rotate instead of press

The press is planar, and any planar path from one side of the chord to the
other must cross it, where `|AB| + |BC| = |AC|`. The deficit
`D = |AB| + |BC| − |AC|` is not a constant: it grows as the loop closes
(`|AC|` shrinks with B's fold), so the ambient grip modulates it, and a
crossing that coincides with a closed moment costs more than any rest-pose
estimate. R1's 102 mm was the *minimum* over the schedule, not the budget.

**A rigid rotation of the triangle about the A–C chord has no deficit at any
angle.** B moves on a circle of radius |h| around the chord axis; `|AB|` and
`|BC|` are constant throughout; the endpoint at 180° is the same mirror R5
already accepted. Target path, replacing `B(s) = B − s·h`:

    n     = u x h_hat                   (unit normal of the A–C–B plane)
    theta = s * pi/2                    (s in [0, 2]: 0 = rest, 2 = mirror)
    B(s)  = foot + |h| * (cos theta * h_hat + w * sin theta * n)

with `w = kKneadDentSwingPm / 1000`. `w = 1` is the rigid circle: zero
compaction, zero stretch, F-A and C-E untouched, A and C pinned, nothing to
duck for. `w = 0` is Revision 1's press. In between, the mid-gesture length
cost is `sqrt(|A·foot|² + w²|h|²) − |AB|`: quadratic in `w`, so half a swing
buys back only a quarter of the compaction — the knob exists for the eye, but
the physics wants it at or near 1000.

**Integer form.** `n` needs a cross product and one `isqrt64`; `sin/cos` come
from `zref::fx_sin` on angle16 exactly as `rear_bow_delta_mm` does
(`theta16 = s_pm * 16384 / 1000`). Two more rounded divides in
`dent_target_mm`, still one function, still int64.

**What it costs instead.** B leaves the loop plane by up to `w·|h|`
(≈ 184 mm est., at θ = 90°). The loop plane is the creature's sagittal plane
and the house camera looks at it from the side, so the excursion is *toward or
away from the viewer* — foreshortened to a ~10 % size change at 240p — while
the visible motion is B descending through the chord line and settling under
it. On Inspect's orbit the swing is visible as a swing. **That is the art
question, and only the eye answers it** (R2.6). Mid-swing the tube's roll
about its own axis is whatever `nodule_aim`'s z-then-x composition produces,
which may not be a natural bend's roll: watch for twist in the front window.

**Depth beyond the mirror: the overpress.** R5 reached strictly lowest on
13/21 at the mirror and 19/21 at s = 3000, because |h| varies with the loop's
openness and on some clips the mirror is not 20 mm under A or C. The rotation
cannot go further than 180° (it comes back up), so extra depth is a straight
continuation from the mirror along −h_hat:

    B(s > 2) = B(2) − (s − 2) * |h| * h_hat * kKneadDentOverpressPm/1000

It stretches A-B and B-C only, and it happens at full envelope, where the
ambient duck is complete, so nothing stacks: est. at one extra |h| (184 mm)
`|AB| ≈ 465` (+125 mm, +368 pm of 480) and `|BC| ≈ 496` (+116 mm, +305 pm of
400). Inside both interior stretch bounds, with the duck now **load-bearing
for the overpress** (not for the pin) and stated as such in its comment.

**Bounds: none change in this revision.** The interior compaction floors are
expression bounds (their comment: the run-length law is the collapse guard;
these bound how far a visible span may telescope before the skin folds), and
the swing does not touch them. If the eye rejects the swing and the press must
ship, the true deficit (re-measured after the walk fix) decides whether those
two floors are asked for; that request would have to be proven by the
posed-surface quantity they proxy — the **front-window rail strain** — with a
positive control, and it never reaches C-E. It is not proposed here.

### R2.3 The implementer's three questions, answered

1. *Where did the 102 mm go?* It was the deficit at the rest fold; the deficit
   is a function of the instantaneous |AC| and the crossing was measured
   through a broken walk. It is moot for the swing (no deficit) and unknown for
   the press until re-measured.
2. *Can the pin hold exactly?* Yes, once the walk reads the right bones. The
   duck-1000 row already shows it holding.
3. *Is a deeper duck the answer?* No — the duck was compensating for the walk.
   It stays as an art knob and as the overpress's stacking guard. Do not scale
   the swallow beats.

### R2.4 (c) The next falsifying experiment — render-free, three runs

Prerequisites (implementer): the shared `loop_walk`, the swing term in
`dent_target_mm`, `kKneadDentSwingPm`, `kKneadDentOverpressPm`; off path still
bytes (4/4 CRCs).

**Run 1 — the swing is rigid and the pin holds.** `dent`, s = 2000,
swing = 1000, overpress = 0, **duck = 0** (honest), whole bank. Per-sample
diff against no-dip from `mspan --csv` and `mrear --dip --csv`:

* F-A: identical on every sample.
* C-E: `max |Δ| ≤ 1 mm` over **all** samples — per sample, not the extremes;
  the extremes hid the per-frame leak last time.
* A-B and B-C: `max |Δ| ≤ 1 mm` over all samples. **This is the new central
  claim** — a rigid rotation leaves every span length exactly as the ambient
  had it. Any larger difference means the rotation is not rigid (a bug) or the
  walk is still wrong.
* R5: Inspect strictly lowest by 20 mm; bank count reported (expect ≈ 13/21,
  the same endpoint as before).
* Reported, not gated: B's out-of-plane excursion per sample (distance from
  the pre-dent A–C–B plane), the G9 step/accel/jerk worsts, and the front
  rail min/max. These are the eye's and the next gate's inputs.

**Run 2 — what the press really costs.** Same, swing = 0, duck = 0 and
duck = 1000: log per-sample `D = |AB| + |BC| − |AC|` and the interior deltas.
This replaces `P20-DENT-EXPERIMENT.md` §3 with a trustworthy number and is the
owner's information if the swing is refused on sight.

**Run 3 — the overpress.** swing = 1000, overpress {500, 1000}, duck = 1000:
R5 count (target: every hosting clip) and A-B/B-C stretch worsts against 480 /
400. If a clip still misses at 1000, report which and its |h|; do not raise the
overpress past the interior stretch bounds.

Falsified if Run 1 fails any bullet. Ten minutes per run.

### R2.5 If the eye refuses the swing

Then B-strictly-lowest on every clip requires a planar press, whose crossing
deficit is paid by the interior spans, and whether it fits inside −330 / −430
is decided by Run 2. If it does not, **the smallest change the owner must
approve is the two interior compaction floors** (A-B and/or B-C), by the
re-measured amount, justified by a front-window rail floor with a fired
control — never C-E, never F-A, never the run-length floor. I am not proposing
it; I am naming it so the choice is visible before the ladder rather than
after.

### R2.6 Gates and look, delta from §10

* **G10 DENT PIN** gains the rigid-swing leg (A-B/B-C ≤ 1 mm with overpress 0)
  and its control `--fail-dent-rigid` = swing 0 (the press: interior deltas
  move by tens to hundreds of mm). `--fail-dent-pin` stays the carried solver
  at gain 1000.
* **Report** B's out-of-plane excursion and the front rail strain per clip;
  floor the rail after the ladder, control `--fail-dent-overfold` (overpress
  3000).
* **Look:** Inspect at 3× through one full swing (12-tile strip), the
  fixed-camera idle at native, a before/after pair against HEAD, and a
  trajectory plot of B's core in Y and Z over the clip — the Z line is the
  swing, and the eye needs to see whether it reads as a press or a flip.

### R2.7 What I would NOT do

Widen any bound; keep two walks; measure anything through the old walk; scale
the swallow beats; take the swing past 180°; make the duck carry the pin; ship
the press with breaches and call the swing "for later".

---

# Revision 1 (superseded where R2 says so; kept as history)

**Date:** 2026-09-20 (architect packet, read-only)
**Branch / HEAD:** `manafold-pass20` @ `0918d554`
**Inputs:** `P20-DIP-STOP.md` (the ledger; this is its option 2), `P20-DIAGNOSIS.md`,
`P20-IMPLEMENTATION.md`, `P20-GATE-CHANGES.md`, Owner Direction 21,
`tools/reel/manafold_clips.h` (`loop_pose`, `nodule_aim`, `finalize_rear_follow`,
`antenna_knead`, `swallow_nodules`, `knead_dip_mm`), `manafold_art.h` (the
span envelope, the dip knobs), `manafold_spangate.cpp` (G5/G6/G7/G9),
`manafold_rear_audit.cpp` (R1/R4/R5).
**Status:** design only. Nothing built, nothing rendered, nothing measured by me
beyond arithmetic on the committed constants. Every number below marked *est.*
is a hand estimate from `kLoopArcMm` / `kLoopFold*A16` and is exactly what the
falsifying experiment (§9) exists to check.

---

## 0. Verdict in one paragraph

Replace the dip's use of the **sequential carried solve** with a **pinned
two-bone re-fold of the A–B–C triangle**: A and C stay exactly where the rest
of the pose already put them (world position *and* frame), and B is pressed
along its own perpendicular to the A–C chord — through the chord and out the
other side, to its mirror image. Only two things in the rig change: the fold
at HingeA and the fold at HingeB (both written by the existing `nodule_aim`),
plus the two interior span deltas (A-B, B-C) that the crossing geometrically
requires. **F-A and C-E are untouched by construction, not by tolerance.**
Closure is inherited unchanged because the frame entering the closure walk is
pinned to its pre-dent value. The mirror pose puts B roughly **137 mm below A
and 234 mm below C** (est.) on every clip, independent of the clip's ambient
pose, because the dent is defined relative to wherever A, B and C are *now*.

## 1. What the ledger's numbers actually say

Before designing anything: the ledger was read as "lowering B costs the
attachment." The committed constants say something narrower and more useful.

**1.1 The shipped dip lowers B's RANK mostly by lifting C, not by lowering B.**
`kKneadDipFoldDeltaPm = {+170, -1000, 0}` × `kKneadDipFoldPm = 2000` drives
`fold_delta_pm[1]` to **−2000 pm** at full envelope, so `fb = kLoopFoldBA16 ×
(1000 − 2000)/1000 = −62°`. A station's fold rotates the span that *leaves* it,
and the span leaving B is B→C. At rest the chain directions from vertical are
neck 8°, after A 48°, after B 110°, after C 180°; with B's fold flipped the
B→C span points at **−14°**, i.e. up and slightly *forward*. C is hoisted
≈ 500 mm above its rest height (est.) and pulled ≈ 450 mm toward the front.
That is where the C→socket chord growth comes from. The implementation's own
fold ladder says the same thing in its shape: fold 0 → 0/21, fold 2000 →
14/21 — the *fold* was buying the ranking, and the fold moves C.

**1.2 B's own descent is capped at 320 mm and is not the attachment cost.**
`swallow_nodules` routes the dip's `b_mm` into `g.nod.by`, and `loop_pose`
clamps it at `kNoduleOffsetMaxMm[1] = 320`. That is why "raising the authored
depth from 470 mm to 1100 mm improved B's rank by 43 mm": everything past 320
was clamped away, and the 43 mm came from the fold side. And lowering B by the
carried solve pushes C *down* with it, shortening the chord: that spends the
**compaction** side of C-E (already at −687 of −700), not the stretch side.

**1.3 The carry cancel cancelled in the wrong space.** `+d` was added to C's
*offset* — a target expressed in the carried frame. But after HingeA is re-aimed
to lower B, the whole downstream chain has *rotated*, not translated; C's carried
position moved along an arc. A pure vertical hand-back leaves the rotation
residual on the B→C span, which then has to stretch and swing to reach a target
that is not where C was. That residual is what tripped G9 (carrier jerk) and G7
(closure) at every amplitude: the cancel was fighting the aim rather than
replacing it.

**So the attachment cost was never intrinsic to "B lowest". It was the
mechanism.** Two of three channels used (fold share, carried offset) move C;
the third (carry cancel) failed to un-move it. A solver that never moves C has
nothing to pay on C-E at all.

## 2. The mechanism: THE DENT

### 2.1 Geometry

Take the three carrier positions the *existing* pose produces on this key —
A, B, C in world millimetres, after the carried nodule solve has run (so ambient
offsets, swallow beats, tilt and yaw play are all already in them). Let

    u    = (C − A) / |C − A|                     chord direction
    foot = A + ((B − A) · u) u                   B's foot on the chord
    h    = B − foot                              B's perpendicular height

The dent target is

    B(s) = B − s · h ,   s ∈ [0, kKneadDentDepthPm/1000]

`s = 0` is the pose as it is; `s = 1` presses B onto the chord (the loop's top
is a straight line A—B—C); **`s = 2` is B's mirror image through the chord**,
and it is free: `|A B(2)| = |A B|` and `|B(2) C| = |B C|` exactly, so no span
is stretched or compacted at the bottom of the dent. B never leaves the plane
of (A, B, C), so tilt/yaw play is honoured, not fought.

Rest-pose estimates (`kLoopArcMm` 680/340/380, folds 8°/40°/62°/70°), in a
(over, up) frame with A at the origin:

| | A | B rest | C | foot | B mirror |
|---|---|---|---|---|---|
| over | 0 | 253 | 610 | 285 | 311 |
| up | 0 | 227 | 97 | 43 | **−137** |

|h| ≈ 184 mm. At the mirror B is ≈ 137 mm below A and ≈ 234 mm below C.
Strictly lowest by `kGateDipMarginMm = 20` needs **s ≳ 1.35** (B passes A at
s ≈ 1.24 and C at s ≈ 0.8). So the shipping depth is a by-eye ladder over
roughly {1400, 1600, 2000}, and the geometry — not the amplitude — is what
makes "every clip" true: the dent is relative to that key's own A and C.

### 2.2 The crossing, and the only bounds it touches

By the triangle inequality any path from one side of the chord to the other
passes through it, and *on* the chord `|AB| + |BC| = |AC|`: with A and C pinned
the two interior spans must together give up `340 + 380 − |AC| ≈ 720 − 618 =
102 mm` (est.) at the crossing and nowhere else. Through the foot that is
≈ −54 mm on A-B (−160 pm) and ≈ −48 mm on B-C (−126 pm); `kKneadDentCrossPm`
slides the crossing point along the chord to redistribute it (equal per-mille
is at ≈ −141/−142 pm). Both are far inside `kSpanCompactionMinPm` (−330,
−430) and leave runs of ≈ 186 / 232 mm against `kSpanMinRunMm = 80`.

This is the design's honest cost, and it is on the **interior** spans, on the
**compaction** side — the ledger's own words for where a knead belongs. The
attachment span does not appear in the arithmetic.

**The one real interaction:** the bank already reaches −107 mm on A-B and
−137 mm on B-C from the ambient nodule schedule and swallow beats. If a
crossing coincides with an ambient compaction extreme, A-B stacks to ≈ −456 pm
against −330. That coincidence is deterministic (both are functions of `slot,
keys, f`), so the ladder will show it, and the design carries a named lever for
it (§2.5, the ambient duck) rather than a hope.

### 2.3 Where it lives and what it writes

Inside `loop_pose`, as a **third block** between the carried nodule solve and
the closure walk, guarded by `if (g.dent_pm > 0)`. It writes exactly:

* `g.q[kBHingeA]` — re-aimed by `nodule_aim` so A→B points at B(s);
  `set_span_delta(1, |A B(s)| − 340)`.
* `g.q[kBHingeB]` — re-aimed by `nodule_aim` so B(s)→C points at **the saved
  world C**; `set_span_delta(2, |B(s) C| − 380)`.
* `g.q[kBHingeC]` — **pinned**: `renorm(conj(Q_enteringC') · Qc_world_saved)`,
  so HingeC's world frame is what it was before the dent.
* `g.span_pm[1]`, `g.span_pm[2]` — the diagnostic receipts, via `nodule_aim`.

Nothing else. Not `kBNeck`, not span 0, not `g.nod`, not HingeD/E/socket, not
the rear helpers. Pseudo-code, in the solver's own vocabulary:

```
// after the carried nodule solve, before the closure walk
if (g.dent_pm > 0) {
  // walk exactly as the closure does: arc[i] + (local_t[child][1]*1000)>>16
  P0 = (kLoopTubeXMm, kLoopNeckExitYMm, 0); Q = q[JF]
  span0 (len 0)  -> Q *= q[Neck]        ; PA = P0 + Q*(0, len1, 0)   [after span 1]
  Q1 = Q*q[HingeA]                       ; PB = PA + Q1*(0, len2, 0)  [after span 2]
  Q2 = Q1*q[HingeB]                      ; PC = PB + Q2*(0, len3, 0)  [after span 3]
  Qc_world = Q2*q[HingeC]                 // frame entering the closure walk
  Bt = dent_target_mm(PA, PB, PC, g.dent_pm, kKneadDentCrossPm)

  NQ = Q1; p = PA
  nodule_aim(q[HingeA], NQ, kLoopArcMm[2], p, Bt, &span_pm[1], &d1); set_span_delta(1, d1)
  NQ = NQ*q[HingeB]
  nodule_aim(q[HingeB], NQ, kLoopArcMm[3], p, PC, &span_pm[2], &d2); set_span_delta(2, d2)
  q[HingeC] = quat16_nlerp(x, x, 1, 2) where x = conj(NQ) * Qc_world   // the pin
}
```

`nodule_aim` already does the two-stage in-plane/out-of-plane aim, reports the
absolute length delta (`want − len`, not incremental), and advances `p` to the
exact target, so the second aim starts from B(s) precisely. The pin uses the
production renormaliser (`rear_socket_compose`'s `nlerp(q, q, 1, 2)`), for the
reason its comment gives: a long quat16 product is not unit and
`quat16_to_mat3` scales by |q|².

Because the closure walk then composes `Q_JF·q[Neck]·q[A]'·q[B]'·q[C]' ≈
Qc_world` and reaches `PC` to angle16 quantisation (≈ 0.05 mm per aim stage),
HingeD's aim, the arm's arrival frame `qd`, the End frame, the rear helpers and
the socket are **the pre-dent values to the LSB** — and `finalize_rear_follow`,
which re-walks the same chain from the same tracks, agrees. That is the sense
in which C-E is untouched *by construction*: the dent has no term in it.

### 2.4 The envelope and the schedule — reused, not rebuilt

Timing stays exactly the shipped dip's: `knead_dip_mm`'s windows
(`kKneadDipRise/Hold/FallPm`, `kKneadDipMinRampKeys`, `kKneadDipMinHoldKeys`,
`kKneadDipCount`, `kKneadDipMinRestKeys`, `kKneadDipPhasePm`,
`kKneadDipSlotSkewPm`), modulo `keys`, C2 via `motion_c2_ease`, per-slot
`kKneadDipClipPm` (Trick 13, Still 7 and Taunt III 21 stay at 0), global
`g_u02_knead_dip_gain_pm`. Factor the window arithmetic of `knead_dip_mm` into
`knead_dip_window_env(slot, keys, f)` (0..1000) and have `knead_dip_mm` call
it — byte-neutral because the integer operations are unchanged — then

    env_pm    = knead_dip_window_env(...) * dip_gain / 1000
    g.dent_pm = env_pm * kKneadDentDepthPm / 1000        // s in per-mille

is set by `antenna_knead` in the place the old dip block sits. `s` is C2 in
time; the aim is a smooth function of B(s) with no singularity on the straight
path (unlike the bow's √ at the taut point), so G9 sees a C2 carrier.

### 2.5 The ambient duck

While the dent is active the ambient nodule offsets on A/B/C are scaled:

    g.nod.{a,b,c}{x,y,z} *= (1000 − env_pm * kKneadDentAmbientDuckPm / 1000) / 1000

before `loop_pose` (so it flows through the same `swallow_nodules`/`g.nod`
path — not a second channel). Named, default 1000 ("the press owns the
carriers while it presses"), laddered 0/500/1000 by eye and by the stacking
numbers in §2.2. F and E offsets are not ducked. This is the only mechanism in
the design that touches anything other than the two interior spans, and it can
only *reduce* an existing excursion. It is preferable to phase-aware scheduling
(hiding a coupling inside the timing) and to a solver clamp (forbidden by the
span law's own comment: "a silent clamp would detach a requested carrier").

### 2.6 Knobs (all named, all in `manafold_art.h`, all with a `g_u02_` twin)

| knob | default | meaning |
|---|---|---|
| `kKneadDipSolver` (`enum {kCarried, kDent}`) | `kCarried` until step 6 of §8, then `kDent` | which mechanism realises the dip |
| `kKneadDentDepthPm` | 2000 to start; by-eye ladder {1400, 1600, 2000} | s at full envelope; 1000 = flat, 2000 = mirror |
| `kKneadDentCrossPm` | 0 | crossing offset along the chord from B's foot, per mille of |AC| |
| `kKneadDentAmbientDuckPm` | 1000 | §2.5 |
| `kKneadDentPinToleranceMm` | 2 | gate constant (§10), not a solver input |
| env `ZHAO_U02_KNEAD_DIP_SOLVER=carried\|dent`, `ZHAO_U02_KNEAD_DENT_DEPTH_PM` | | ladder / control, parsed as §3.4 requires |

The carried solver's own knobs (`kKneadDipRank`, `knead_dip_carrier_mm`,
`kKneadDipDepthMm`, `kKneadDipOuterLiftPm`, `kKneadDipCarryCancelPm`,
`kKneadDipFoldDeltaPm`, `kKneadDipFoldPm`) stay exactly as they are and are
inert under `kDent`; their comments gain one line saying so.

## 3. Fixed-point and integer reality

* **Units.** Positions and lengths are world millimetres in `int32` (the chain
  walk already is); span deltas are fx16 via `fxu`; angles are angle16.
  `dent_target_mm` does its dot product and projection in `int64`: |C−A| ≤
  ~1500 mm, so `(B−A)·(C−A)` < 5·10⁶ and the projection numerator
  `dot · (C−A)_i` < 8·10⁹ — needs `int64`, comfortably inside it. `|C−A|` via
  the existing `isqrt64`. The `s·h` product is < 2000 × 400.
* **Rounding.** One rounded divide for the foot (`(dot·(C−A)_i + den/2)/den`)
  and one for `s·h/1000`, magnitude-symmetric like `signed_scaled_fx`. Write
  them once in `dent_target_mm`; never inline. Sub-mm error is irrelevant to
  the look and to every gate (the tightest is G7 at 10 mm), but the rounding
  must be *the same everywhere*, which it is if there is one function.
* **Angle quantisation.** `angle16_of` uses `asin16` on a ratio scaled by
  60000; near |x|→1 it is ill-conditioned (≈ 0.07°), i.e. ≈ 0.5 mm at the far
  end of a 380 mm span. `nodule_aim` already lives with this at three stations.
* **Quaternion drift.** The pin is the one *new* long product; renormalise it
  (§2.3). Do not renormalise `q[HingeA]`/`q[HingeB]` — the existing solve
  doesn't, and changing that would move bytes on the off path.
* **The `>> 16` of a negative delta** floors; the closure walk and
  `finalize_rear_follow` both already do it that way, and the dent's walk must
  copy the expression verbatim so all three agree on `len`.
* **Determinism.** No floats, no `getenv` inside the solver, no per-binary
  arithmetic. The keys carry the result; midpoints derive by the existing
  `finalize_rear_follow_midpoints` averaging/nlerp with no special case, exactly
  as the carried nodule solve already does.

**3.4 The knob must reach every binary that builds a bank.** Today
`ZHAO_U02_KNEAD_DIP_PM` is parsed in `zhao_reel.cpp` and
`manafold_rear_audit.cpp` only — `manafold_spangate.cpp`, `manafold_probe.cpp`,
`manafold_nodule.cpp`, `manafold_public_jointgate.cpp`, `manafold_qa_p12.cpp`
and `manafold_bandprobe.cpp` do not see it, which is precisely the inert-ladder
trap `P20-GATE-CHANGES.md` records. The solver toggle and depth go into **one
shared parser** (`u02::apply_knead_dip_env()` in a header, called by every
`main` that builds clips), and step 1 of §8 proves each binary moves a number
when the env changes.

## 4. Compatibility — the non-negotiable control

* **Toggle:** `kKneadDipSolver == kCarried` (compile default until the final
  step; env `carried` at any time).
* **"Off" means, precisely:** `g.dent_pm` is 0 on every key of every clip, so
  the guarded block in `loop_pose` executes **zero instructions**, the ambient
  duck multiplies by 1000/1000 (guard it with `if (env_pm)` so it too executes
  nothing), and `antenna_knead`'s existing dip block runs *unchanged* at
  `kKneadDipGainPm = 550`. `knead_dip_mm`'s refactor is instruction-identical.
  Therefore every track of every clip is byte-identical to HEAD `0918d554`.
* **Receipts:** record the shipping CRCs (dip on, gain 550) for hover,
  inspect, taunt3 and still **from the HEAD binaries before touching a file**;
  after steps 1–2 of §8 they must match. `P20-IMPLEMENTATION.md §4` records
  `0xA2D0E051` / `0x779615BB` / `0x75BC4777` only for the *dip-off* state, so
  a fresh dip-on triple is needed — and the pass-19 CRC discrepancy noted there
  means the like-for-like baseline is *this tree's* binary, not a document.
* **Under `kDent`, two positive identity controls:** Taunt III (slot 21) and
  Still (slot 7) author no dip, so their clips must be byte-identical between
  solvers. If either moves, the dent leaked through a path other than
  `g.dent_pm`.

## 5. What the existing bounds mean now

| item | as written | under the dent |
|---|---|---|
| `kSpanStretchMaxPm` / `kSpanCompactionMinPm` | envelope on shipped tracks | **unchanged and still correct** — they bound tracks, not a solver. The dent can only compact A-B and B-C (never stretch below s=2, never touch F-A/C-E). *Repair the comment* at `manafold_art.h:2337–2353`: it describes 480→490 / −430→−450 values that the arrays do not contain (`P20-GATE-CHANGES.md §7` says they were rejected). A comment that contradicts its constant is the wrong-number-with-reassuring-provenance trap. |
| C-E `+440` (the attachment bound) | untouched | **untouched.** Gate that the dent leaves the C-E delta within ±1 mm of the dent-off value (§10) — that is the stronger claim, and the one the ledger asked for. |
| `kSpanMinRunMm = 80` | attachment/dongle guard | unchanged; worst run under the dent ≈ 186 mm (est.). |
| Signed-span helpers `kBSpanDeltaB/C` and `span_helper_delta_fx` | constant-slope fraction of the child delta | unchanged law; they now carry the dent's compaction. G5's fraction check passes by construction because the dent goes through `set_span_delta`. |
| `kNoduleOffsetMaxMm[1] = 320` | bound on a carried *offset* | no longer what limits B's travel during the dip (the dent targets world space, not an offset). Its comment should say the 470→1100 "43 mm" result was this clamp. |
| Attachment guard (`G5` run margin, `mprobe` burial) | as is | as is. Must stay green; nothing in the rear changes. |
| R1 (arm↔End 40°, centreline 140°) | rear frame | unchanged, and the rear *numbers* must be identical dent-on vs dent-off (§10). |
| R4 STRAIN (rear rail 0.40/2.10, step 0.18) | rear window | unchanged thresholds; rear numbers identical. The dent's strain is in the **front** window (the fold at A goes ~40°→~105°, at C the in-bend ~70°→~110°, est.). R4 already computes `front_rail_min/max` but does not gate them: **report them under the dent, then floor them** (§10). |
| R5 DIP (margin 20 mm, return 60 mm) | "reached lowest" reported, "happened and returned" hard | **promote "reached lowest" to hard** on every hosting clip. The geometry makes it reachable on all of them; a clip that does not is a defect, not an art value. |
| mspan G5 E-stage | helpers == production writer | unchanged; the ±1 mm chord ambiguity already covers the pin's LSB drift. |
| mspan G6 (turn < 140°, pinch) | ring order | unchanged; now exercised at A's deep fold. |
| mspan G7 (endpoint 10 mm) | closure | unchanged; must stay green — the leg the carry cancel broke. |
| mspan G9 (80/60/60 mm, 8/6/6 °) | carrier continuity | unchanged. Est. peak at the 9-key ramp floor: B ≈ 30 mm/sample, HingeA ≈ 5°/sample. If a short clip breaches, raise `kKneadDipMinRampKeys`, never the ceiling. |
| G8 crown shuffle | Taunt III | untouched (slot 21 has no dip). |
| `kFoldDipRefMm = 420` (item-3 particle read) | mm of B's sag below the A/C line | still correct in *kind* — it reads the pose, so it reacts to the dent for free — but the dent's full sag is ≈ 2·184 mm below the line at the mirror, so the reference is now an art value to re-ladder by eye. Not a solver matter. |

## 6. Alternatives rejected, and why

* **Global/iterative constraint solve over the closed loop (FABRIK/CCD with the
  socket pinned).** Closure by iteration is closure by tolerance; the house
  already has closure by construction (D's aim) and a non-iterative integer
  culture ("closed-form pose arithmetic, not IK"). It also would change bytes on
  the off path unless bypassed entirely, at which point it is this design with
  more code.
* **Arc-length redistribution along the whole loop.** Moves C and F by
  construction, which is the fault. Also blurs the owner's carriers into a
  continuum they explicitly wanted to own individually.
* **Carry cancel done right (aim C at its saved world position, keep B's fold
  share).** Half of this design. Rejected as a whole because the fold share
  still hoists C and the interior spans then have to *stretch* to reach a
  pinned C — the −442/+482 breaches. Pinning C's frame *and* letting the two
  aims choose the folds removes the stretch: at the mirror there is none.
* **A dedicated vertical bone/channel on B.** A new bone changes the skin
  palette, the model, every gate's station bookkeeping and the silicon bone
  budget, for a motion the existing two hinges can already express.
* **Relaxing C-E.** Refused by the owner and the ledger; and unnecessary — the
  dent's C-E cost is zero, so relaxing it would buy nothing.
* **Depth beyond the mirror (s > 2).** Requires both spans to stretch, which
  re-enters the envelope fight for no visual gain; kept reachable by the knob
  and used only as the over-fold positive control.

## 7. Risks, in order of what I expect to bite

1. **The look.** A V at the top of the loop, with ≈ 105° at the front hinge
   (est.), may read as a *broken* antenna rather than a knead. This is the
   biggest risk and only the eye settles it: the by-eye ladder is over depth
   {1400, 1600, 2000} on Inspect at 3× on the dent window, plus a 12-tile
   strip across the press and a before/after pair against HEAD. Frames chosen
   by badness (worst front rail strain), not by index.
2. **Skin at the deep folds.** The inside of A's fold and C's in-bend may pinch
   (rail min in the front window) or the outside may over-stretch. G6 pinch and
   the new front-rail floor watch it; the fold blends are 90 mm, the balls
   50 mm half-core, and the rear band already passes at 113°.
3. **Ambient stacking at the crossing** (§2.2). Deterministic; the ladder shows
   it; the duck removes it. If the duck at 1000 still leaves a breach, the
   ambient itself was over its own bound at that key and the answer is in the
   ambient schedule, not in a wider bound.
4. **G9 angular step on short clips** (§5). Fix by the ramp floor, not the
   ceiling.
5. **The pin leaking.** If `finalize_rear_follow`'s re-walk and `loop_pose`'s
   walk disagree by more than quantisation, C-E moves and the DENT PIN gate
   fires. The falsifying experiment measures exactly this first.
6. **Trick's plant** — Trick stays at `kKneadDipClipPm[13] = 0`. Unchanged.
7. **The pass-20 bow fix** — the rear tracks are byte-identical dent-on vs
   dent-off (they have no dent term), so it cannot regress by construction;
   gated anyway (§10).
8. **One-shots with `hold_last`.** The schedule already refuses a window that
   would leave B down (`win >= span → 0`); confirm the last key of every
   one-shot has `env = 0` so the held tail does not freeze a dent.

**What I would NOT do:** widen any bound; touch `finalize_rear_follow`, the
bow, the rear helpers, the socket, HingeD or E; move C or A at all; keep the
fold share or the carried B offset alive under `kDent`; clamp in the solver;
iterate; use floats; add a bone; schedule the dip around the ambient's phase;
refactor the closure walk in this packet (add the dent's own walk, dedupe
later behind the CRC gate); re-tune R1/R4 thresholds; touch Taunt III's crown
shuffle; render/encode/deploy the bank as part of this packet.

## 8. Staged build order

Each step ends in a commit; steps 1–2 must not move a single byte.

0. **Baseline receipts from HEAD binaries:** dip-on CRCs for hover / inspect /
   taunt3 / still; `mspan --csv` of the four signed spans for the whole bank;
   `mrear --gate --dip` R5 counts. This is the like-for-like reference.
1. **Knob plumbing.** `kKneadDipSolver`, `kKneadDentDepthPm`,
   `kKneadDentCrossPm`, `kKneadDentAmbientDuckPm`; the shared env parser;
   `Rig::dent_pm` (reset to 0 in `reset()`). Prove every bank-building binary
   moves a number when `ZHAO_U02_KNEAD_DENT_DEPTH_PM` changes *with the solver
   set to dent* — and that with `carried`, CRCs equal step 0.
2. **The solver block** in `loop_pose` + `dent_target_mm` + the
   `knead_dip_window_env` factor + the `antenna_knead` branch. Default still
   `kCarried`. CRCs equal step 0. Commit: "the off path is bytes".
3. **Falsifying experiment** (§9). If it falsifies, stop and write the ledger.
4. **Look and ladder.** Depth, cross, duck — by eye, on Inspect, native and 3×,
   before/after against HEAD. Choose values. Record the ladder in the constants'
   comments, as the pass-20 knobs do.
5. **Gates** (§10) with their positive controls fired *before* their silence
   is quoted; matrix.
6. **Flip the default** to `kDent` at the chosen depth; full matrix from
   scratch in one invocation; commit; update `P20-IMPLEMENTATION.md §2.5` and
   `P20-GATE-CHANGES.md` with the rows this adds.

## 9. The falsifying experiment (cheapest, earliest)

With step 2 built and nothing else: `ZHAO_U02_KNEAD_DIP_SOLVER=dent`,
`ZHAO_U02_KNEAD_DENT_DEPTH_PM=2000`, on **Inspect only**, using tools that
already exist:

1. `manafold-spangate --csv` — per key/midpoint F-A, A-B, B-C, C-E signed
   deltas, dent vs carried-at-gain-0 (i.e. no dip at all).
2. `manafold-rear-audit --gate --dip --csv` — R5 rank margins and the rear
   rail/hoop/handoff numbers, same two builds.

**The design is falsified if any one of these holds:**
* F-A delta differs on any sample (it has no dent term — a difference is a
  leak);
* C-E delta differs by > 1 mm on any sample, or any rear R4 number differs
  beyond the third decimal (the pin failed);
* A-B or B-C compaction at the crossing exceeds ≈ 170 pm *with the ambient
  fully ducked* (the geometry estimate is wrong);
* R5 does not report B strictly lowest by 20 mm on Inspect at s = 2000 (either
  the ring the gate reads is not the ball, or the mirror estimate is wrong).

Ten minutes, no render, no new tool. Only after it passes is any skinning,
gate or eye work worth doing.

## 10. Gate plan

Measured on **posed surface** quantities (ring centroids of the *visible ball
cores* via `visible_core_centroid`, the rail strain, the shipped tracks), never
on bone origins — pass 19's blindness and R5's own first-version lesson.

| leg | what it asserts | positive control (must fire, declared mask) |
|---|---|---|
| **mspan G10 DENT PIN** (new) | For every key/midpoint of every clip, dent-on vs dent-off (same process, knob toggled, bank rebuilt — the `--fail-rear-strain` pattern): posed A-core and C-core centroids within `kKneadDentPinToleranceMm = 2`; F-A track identical; C-E track within 1 mm; all six rear helper tracks and HingeD/RearSocket quats within 1 fx-mm / 1 LSB. | `--fail-dent-pin`: the carried solver at gain 1000 — the ledger's own defect (C-E +529). Fires by hundreds of mm. |
| **R5 DIP, hard** | Every hosting clip: B's posed core below both A's and C's by `kGateDipMarginMm` on at least one key, *and* returns (existing 60 mm). | `--fail-no-dip` (existing, mask 0x10). |
| **R4 front window** (extend) | Front rail min/max and hoop reported per clip; after the ladder, a regression floor `kGateFrontRailRegressFloor` set below the shipped worst with margin (the 0.12→0.40 lesson: never below the defect). | `--fail-dent-overfold`: depth 3200 (past the mirror: both spans stretch, A's fold > 130°). Declared mask includes G5 bounds honestly, as `--fail-rear-strain` declares 0x9. |
| **R4 rear identity** | Rear rail/hoop/handoff/span excursion identical (3 dp) dent-on vs dent-off. | Same control as G10. |
| **CRC identity** | `carried` → HEAD CRCs (4 subjects). Under `dent`: Taunt III and Still byte-identical to `carried`. | A deliberate 1 pm dent on slot 21 in the control build must change its CRC — proving the identity leg can see a leak. |
| G5 / G6 / G7 / G9 / G8 / mprobe / R1 / R2 | thresholds unchanged; all green with the dent shipping | existing controls unchanged |
| **Look receipts** (not a gate, but required) | Inspect dent window at 3×; 12-tile strip across the press; before/after vs HEAD; worst-front-strain frame at 4×; a trajectory plot of B's core height over the clip (a flat line is "it never dipped"). | — |

A gate that reads zero on the pin is a claim: G10's control is fired *first*,
in the same matrix invocation, every time.

## 11. Declared omissions

* **No small A lift.** The crown's "A high" and `kKneadDipOuterLiftPm` are not
  carried into the dent (A is pinned). F-A sits at +293 of 320, so any lift is
  an envelope question the owner's eye should ask for, not one to pre-spend.
* **No re-ladder of `kFoldDipRefMm`** — the particle reaction reads the pose
  and will react to the dent; its reference depth is an art value for the eye.
* **No refactor of the closure walk** into a shared helper in this packet.
* **No change to the ambient nodule schedule or the swallow beats**, even if
  the stacking ladder shows one of them at its own bound — that is a separate
  finding to report, not to fix here.
* **No bank render / encode / deploy.** The packet ends at a green matrix and
  the look receipts; publishing is the finished-pass call.
* **The 8 % gap between my rest-pose estimates and the posed reality** (rest
  tilts, ambient offsets, the +0…−24 mm rest excursion) — every mm above is
  *est.* and the experiment in §9 replaces it with a measurement.
* **The pass-19 CRC discrepancy** (`P20-IMPLEMENTATION.md §4`) is not chased
  here; this design's baseline is this tree's HEAD binary.
