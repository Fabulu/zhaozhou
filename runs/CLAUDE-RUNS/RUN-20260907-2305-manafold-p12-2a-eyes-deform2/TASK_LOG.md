# Task Log: RUN-20260907-2305 - [Describe objective here]

**Created:** 2026-09-07 23:05 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260907-2305-manafold-p12-2a-eyes-deform2/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-07 23:05 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260907-2305
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

## 2305 — lane opened
Clones: `manafold-p12-2a/{zhaozhou,Upheaval}` from origin/main
(zhaozhou d820b574, Upheaval a9e532e). Read D9 §6/§6.1/§6.2/§12/§13,
PASS-12-PLAN §5 (Wave 2a), and Wave 1's own handoff note in
`manafold_art.h` at `kLoopStretchStrength` — which names the exact
prerequisite this lane exists to build.

## 2320 — ITEM 1: the second deform sub-channel, engine half
`DeformSample` could not carry a per-span pose-derived quantity: ONE
{flatten,spread} per key per clip, read by every vertex through a static
`strength`. Built LANES instead of a second channel, because "a second"
would have been a select and a select does not compose:

* `kDeformLaneCount = 5`; `DeformFrame` = every lane of one key.
* `Clip::deform_ex` / `mid_deform_ex` carry lanes 1..4, frame-major.
  Lane 0 stays in `deform`/`mid_deform`, unrenamed and unmoved.
* `DeformVertex::strength_ex[]` / `RingSpec::deform_strength_ex[]` — an
  authority PER LANE. Total deformation is the SUM of the lanes'
  weighted deltas, one rounding per lane, same order as before.
* IDENTITY IS ARITHMETIC, NOT A PROMISE: a vertex with authority on
  lane 0 alone executes one term identical to the old expression, and
  every zero term is skipped before any fixed-point round.
* `deform_skin_vertex_lanes` is a NEW NAME, not an overload: several
  pinned Zixxtrixx probes call `deform_skin_vertex(v, meta, {})` and a
  braced `{}` against two class types is ambiguous — an overload would
  have broken the CRC harness in the files whose job is proving nothing
  moved.
* Two things the single-lane form got for free and the sum does not:
  the positive-volume invariant (now clamped explicitly) and one
  interpolation law per key (extra lanes now bake the same Catmull-Rom
  as lane 0, instead of falling back to a linear average beside it).

Built `cel` clean, rc 0.

## 0010 - ITEM 1 DELIVERED: the spans are distance-driven
nodule_aim was throwing the number away. It aims at the target and then
advances the chain by the BIND arc length, so a ball whose target is further
off than its span is long simply never arrives -- and nothing downstream
knew by how much. It now reports that shortfall in per-mille, measured on
the FORWARD-WALKED POSED chain (no matrix is inverted anywhere, so gotcha
S15's bind-space trap never opens), and each span spends its own on its own
lane. kSpanStretchMaxPm = 300, kSpanThinRatioPm = 600.
kLoopStretchStrength -> 0: the breath drive is retired but kept live as a
knob, because a breath term underneath a distance drive would stretch spans
when nothing moved apart.

Ring authorities are GEOMETRY: a ring carries its own span's stretch AND the
accumulated stretch of every span below it, 255*clamp(s-start,0,L)/s per
lane. That is why lanes had to SUM -- a ring in span 3 needs k1, k2 and k3
at once, and a select would have torn the other two.

## 0025 - THE GATE, and the blind spot it closes
manafold_nodule.cpp reported ball A's vertical reach as -7..+3 mm with the
stretch fully live, and it was right to: posed_ball() skins a synthetic
vertex at the BONE's bind origin with no deform metadata, so a bone gate
cannot see a vertex effect. Same blind spot wave 1 named for the closure
probe. Left that gate alone (protected, and its independence verdict is
correct) and wrote manafold_spangate.cpp (mspan), which reads the SKIN
through decode_pose -> deformation_frame -> deform_skin_vertex_lanes ->
skin_vertex.

    G1 rest identity   925 deform vertices, 0 moved
    G2 no fold-back    0.00 mm reversal at the ceiling
    G3 buried tip      0.00 mm (max 1.00)
    G4 ball A vertical SKIN reach   37.8 -> 172.3 mm, gain 134.5 (floor 60)

THREE FAILABLE LEGS, each witnessed failing:
  --fail-nolanes      G4 fails, gain 0.0
  --fail-steepramp 5  G2 fails, 12.02 mm reversal at station 1720
  --fail-noramp       G3 fails, tip moves 416.56 mm

TWO THINGS THE LEGS FOUND that no amount of reading would have:
  * --fail-ceiling 1200 PASSED, and that was the bug. 1200 pm is 78643,
    which wrapped in the u16 lane sample to 13107 and walked the check at
    200 pm -- LOWER than the default. So a lane cannot express more than
    1000 pm, which in turn makes G2 UNFAILABLE BY ANY VALUE: the bound is
    sum(L_i)=1400 < total-stC=1540 at the structural maximum. G2 guards a
    GEOMETRY change, not a value, and its leg steepens the taper instead.
  * --fail-noramp first passed too. The ramp does not reduce authority
    past hinge C, it zeroes it, so those rings compile role-kNone with a
    zeroed centre and there was nothing left to scale. The leg has to bring
    them back to life, and does so explicitly.

## 0035 - protected invariants, verified not asserted
  zixx-golden before(d820b574) vs after: 46 files, byte-identical --
    every clip payload and every per-key pose CRC
  zixx-probe PASS . zixx-meshcheck OK
  manafold-probe rc 0 (5d gate A 22 mm, gate B 837 pm) .
  manafold-meshcheck CLEAN . manafold-nodule PASS 0 failures
