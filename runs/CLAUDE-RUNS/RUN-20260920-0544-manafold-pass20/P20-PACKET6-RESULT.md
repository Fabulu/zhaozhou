# Packet 6 — the walk, the swing, and what the eye said

**Date:** 2026-09-20
**Design:** `P20-SOLVER-ARCHITECTURE.md` revision 2 (`03db31f3`), sections R2.1–R2.6
**Built:** one shared `loop_walk`; the swing (`kKneadDentSwingPm`) and the
overpress (`kKneadDentOverpressPm`); mspan **G11 WALK PAIRING** with its own
mutant control; the committed pin probe.
**Gate matrix:** 149 legs, 149 PASS, 0 FAIL (`P20-RECEIPTS/gate-matrix.txt`).
**Ship state:** the dent is still **OFF** (`kKneadDipSolver = kCarried`). The
shipping bank is byte-identical: hover `0x79D3F0C5`, inspect `0x0710E704`,
still `0x138FE8B0`, taunt3 `0xC81598AA` — 4/4.

---

## 1. The walk: what was the defect and what was not

The architect was right about the defect and wrong about what it cost.

`loop_walk` is now the single chain walk, factored out of the closure walk
verbatim and called by the closure and by the dent. Segment `i` is
`kLoopArcMm[i]` plus the span delta carried on `span_child[i]` — **the bone that
ENDS it**. The packet-5 dent had its own copy that was one bone early on all
three spans; the probe measured that copy putting **C up to 344 mm** away from
the closure's C, on **3365 of 3424** dent samples.

Corrected against old, same binary options, dent = press, depth 2000, duck 0
(the honest setting), whole bank, per sample:

| quantity | old walk | corrected walk | no dip |
|---|---|---|---|
| F-A envelope | −198..+199 | −198..+199 | −198..+199 |
| A-B envelope | −159..+164 | −217..+164 | −105..+164 |
| B-C envelope | −234..+145 | −256..+145 | −163..+145 |
| C-E envelope | −698..**+324** | −687..**+304** | −687..**+305** |
| bound/margin breaches | 286 | **255** | 0 |
| worst per-sample ΔC-E | **−157 mm** | **+21 mm** | — |
| worst per-sample ΔA-B | −242 mm | −144 mm | — |
| worst per-sample ΔB-C | −222 mm | −110 mm | — |

With the duck at 1000: breaches 94 → **147**, C-E stretch +279 → +274.

**So: the C-E leak was the walk. The interior compaction was not.**

* The "+19 mm on the stretch side, −11 mm on the compaction side" is gone. The
  C-E envelope is now at or inside the no-dip envelope on **both** sides with
  the duck off, and the worst per-sample excursion fell from 157 mm to 21 mm.
* The 3× interior compaction is **real physics of a planar press**. Corrected,
  the extremes are *worse* (−217 / −256 mm), not better: the old walk was
  measuring a misplaced triangle that happened to be gentler. 255 breaches
  remain at duck 0 and 147 at duck 1000.

### The 21 mm that is left is the aim primitive, not the pin

A committed probe (`-DZHAO_P20_PINPROBE`) walks the chain again after the pin
and measures how far C actually moved: **38 mm (L1) worst** at depth 2000, while
B is reproduced from the aim to **3 mm** and the aim lands on its own target to
6 mm. The pin's structure is sound; `nodule_aim` cannot hit the target more
precisely than that, because its z-then-x aim goes through `asin16`, which is
ill-conditioned near its pole — and the dent sends it there. Renormalising every
quat the aims touch (now done, for the reason `rear_socket_compose` gives)
changed nothing, which is how we know it is the angle and not the norm.

**The architect's ≤ 1 mm criterion is not reachable through this primitive.**
The criterion that *is* meaningful for the owner's constraint — the attachment
must not be charged more than it already is — passes: the C-E envelope never
exceeds no-dip.

---

## 2. The swing: the central claim holds

`kKneadDentSwingPm` (1000 = the rigid circle, 0 = packet 5's press, bit for
bit). At swing 1000, duck 0, depth 2000, whole bank, per sample:

| criterion | result |
|---|---|
| F-A identical | **0 mm** on every sample ✓ |
| A-B within tolerance | worst **3 mm** ✓ |
| B-C within tolerance | worst **5 mm** ✓ |
| C-E ≤ no-dip | envelope −687..**+304** vs no-dip −687..**+305** ✓ |
| bound/margin breaches | **0** (the press: 255) ✓ |

A rigid rotation about the A–C chord costs the interior spans nothing, exactly
as designed, and the residuals are the aim primitive's resolution. The trade
curve at depth 3400 is clean and monotone:

| swing | breaches | worst angular step on B |
|---|---|---|
| 0 (press) | 44 | 7.59° (= baseline) |
| 250 | 23 | 12.45° |
| 500 | 11 | 18.36° |
| 750 | 3 | 29.64° |
| 1000 | **0** | **74.96°** |

**And that second column is the problem.** G9's carrier continuity ceiling is
8°/6°/6° (step/accel/jerk) and the bank already sits at 7.59/5.28/5.25. Every
swing setting at every depth measured 55–80° of angular step on carrier **B**,
while the *position* step is untouched. That is a **roll flip**: `nodule_aim`
parameterises its aim as z-then-x, which is degenerate when the target leaves
the parent's XY plane — precisely where the swing sends B. The architect named
this risk in R2.2 ("the tube's roll about its own axis is whatever
`nodule_aim`'s z-then-x composition produces"). It is not depth-dependent and it
cannot be tuned away with the existing primitive.

**The overpress buys nothing.** At depth 3000, overpress {0, 500, 1000} leaves
the R5 count unchanged (2 clips never lowest, worst margin −191 mm, slot 15) and
overpress 1000 adds **379** bound breaches. It stays at 0 and is not proposed.

---

## 3. The ranking is reachable, and shallower than the design thought

R5, swing 1000, per depth (shipping gain 550):

| depth | s at peak | clips where B never becomes lowest | breaches |
|---|---|---|---|
| 1940 | ≈0.80 | 6 of 21 | 0 |
| 2200 | ≈0.91 | 3 | 0 |
| 2425 | ≈1.00 | 3 | 0 |
| 2700 | ≈1.11 | 2 | 0 |
| 3000 | ≈1.24 | 2 | 0 |

**18 of 21 clips reach B strictly lowest at s ≈ 1.0** — the architecture's
estimate was that s ≈ 1.35 was needed, from rest-pose arithmetic. The posed
chain gets there earlier.

---

## 4. The look, which is the verdict

Inspect and Hover, native render, 5–6× crops of the loop, frames chosen by
largest change from the shipping bank (not by index).

* **s ≈ 1.4 and deeper (depth 3400+): NO.** The loop's apex stops being a tube
  and becomes a **hard rectangular slab** with a vertical left wall — a flat
  facet where the arch's rounded peak was. On both witnesses.
* **The press does exactly the same thing** at the same depth (the w = 0 tile is
  indistinguishable in this respect). So the slab is **the fold**, not the
  swing, and not the out-of-plane read the coordinator was watching for.
* **s ≈ 1.0 (depth 2425): acceptable.** The apex lowers and broadens and still
  reads as a continuous tube pressed down in the middle.

Plates: `P20-LOOKS/p20_depth_ladder.jpg` (Inspect, no-dip / s≈1.0 / s≈1.4),
`P20-LOOKS/p20_hover.jpg` (Hover pair), `P20-LOOKS/p20_w_ladder.jpg` (press vs
half swing vs full swing at s≈1.4).

---

## 5. Where this leaves the owner's requirement

One configuration is close: **swing 1000 at s ≈ 1.0** — 18 of 21 clips with B
genuinely the lowest ball, **zero** span bound breaches, C-E never charged
beyond its no-dip envelope, and a read that does not break. Its single blocker
is the **roll discontinuity in `nodule_aim`**, which fails G9 by roughly 7×.

That is a repair to the aim primitive (a roll-stable aim — carry the previous
frame's roll through, or aim with a twist-free rotation instead of z-then-x),
not a new gesture design, not a bound, and not anything the owner has to
approve. It is the next packet.

**No bound was relaxed in this packet.** `kSpanStretchMaxPm` and
`kSpanCompactionMinPm` are untouched, C-E included.

---

## 6. Gate changes

* **mspan G11 WALK PAIRING** (new, `kCatWalk`). Feeds distinct per-span deltas
  (125 / 375 / 750 mm — multiples of 125 so the fx16 round trip is exact) into a
  rest rig and requires every reconstructed segment to be `kLoopArcMm[i]` plus
  *that* span's delta. Control `--fail-walk-pairing` runs the packet-5
  one-bone-early walk against the same rig: **4 of 5 segments mispaired, worst
  +750 mm, attributed (0x8000)**. Normal: 0 of 5, +0 mm.
* **Four new strict selectors** in the matrix: an unknown solver name, swing
  1001, overpress 3001, depth 6001 — each must exit RC 2.
* **`ZHAO_U02_KNEAD_DENT_DEPTH_PM`'s range went 4000 → 6000**, because `s` is a
  product of four per-mille factors and the mirror is unreachable at the
  shipping gain below 4850. A knob whose useful setting is outside its own range
  is the inert-control trap wearing another costume. This is a *range*, not a
  bound: no gate reads it.
* Nothing else moved. No ceiling, floor or tolerance was changed.

---

## 7. Identity receipts

* **Shipping path, 4/4 byte-exact** against the packet-5 baseline: hover
  `0x79D3F0C5`, inspect `0x0710E704`, still `0x138FE8B0`, taunt3 `0xC81598AA`.
  The walk refactor, the swing, the aim renormalisation and G11 are all inert
  with `kKneadDipSolver = kCarried`.
* **Legacy toggle (`ZHAO_U02_REAR_BOW=legacy`), 3/3 byte-exact against the
  immediately preceding commit `b7c096c2`**, verified by building that tree in a
  worktree and rendering the same three subjects with the same environment:
  hover `0xE6DD5EBA`, inspect `0xDE1F5918`, taunt3 `0x75BC4777`.
  ⚠ Two of those differ from the numbers in the packet-4 receipt
  (`0xA2D0E051` / `0x779615BB`); taunt3 matches all the way back. Those two
  values describe an **earlier tree** than `b7c096c2` — the drift is not in this
  packet, and it is recorded here rather than papered over by quoting the
  matching one.
