# P20 solver architecture: THE DENT — a pinned re-fold of the A–B–C triangle

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
