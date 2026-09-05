# Creature 02 mana — the hardware asks (pass 2)

**Date:** 2026-09-05 · **From:** the creature-02 pass-2 implementation
(`RUN-20260905-0207-u02-pass2-implementation`), executing
`RUN-20260905-0157-u02-pass2-architecture/PLAN.md` §6 against
`Upheaval/creature/Unnamed02/OWNER-DIRECTION-2-2026-09-04.md`.

**The governing principle is Direction 2 §0:** *"treat the specs of the
machine like the specs, not what's there now."* The reel now ships actual
lightning mana and a stamp-trail smear, authored so both migrate onto the
specified blocks unchanged. What those blocks still need is stated here
honestly, the way the additive light term was
(`reports/CREATURE-LIGHT-ADDITIVE-TERM.md` — that route works and is
approved).

---

## 1. `FORGE.PRIM` ribbon family — the WHOLE block is unbuilt

This corrects `ADDLIGHTNING.md`'s overstatement on the record. That report
says *"the present RTL is real, but it currently owns topology only"* and
scopes the remaining work as *"chiefly the position evaluator"*. **Neither
is true today** (verified by the pass-2 effects recon):

* `fpga/rtl/forge/` contains exactly one module, `zhao_forge_cliff.sv`
  (FORGE.CLIFF). **No ribbon RTL exists.**
* **No `zref::ForgePrim` reference oracle exists** anywhere in `reference/`
  or `tests/`.
* `FORGE.PRIM` maturity is SPECIFIED with an **empty maturity log**, and its
  named tests do not exist.

So the ask is the **whole block**: the topology generator, the parameter
block, the position evaluator, the `zref::ForgePrim` oracle, and the
directed/random tests. The contract's frozen limits stand
(`design/contracts/FORGE.PRIM.md`: six families including ribbon,
`MAX_SEGMENTS = 64`, `MAX_SIDES = 8`; a primitive owns no memory — 8
parameters of bandwidth instead of 1,024 triangles of vertex data).

**Cost when built** (arithmetic — no fragment counter exists): a 24-segment
3 px-wide bolt is 48 triangles and ~400 px, **0.4% of one full-screen pass**.
Lightning is a block problem, never a fill problem.

## 2. `FX.LIGHTNING` as an effects-level contract

Not a seventh Forge family — an effects-level contract above the ribbon,
with the field list `ADDLIGHTNING.md` already names: start/end anchors,
tick, seed, segments, jitter, width, colour, intensity, branches, lifetime,
semantic weight, viewport mask. Bounded at **24 segments + 2 branches of
8**. LOD rides the frozen `PART.LADDER` ruling (2026-08-31 §2.5): ribbon →
soft sprite → glint, chosen per camera.

**The authoring is already written and shipping.** `unnamed02_fx.h`'s
`bolt_path()` implements the FX.LIGHTNING recurrence verbatim —
`P_i = lerp(start,end,i/N) + perp1·jitter(seed,phase,i) +
perp2·jitter(seed²,phase,i)` — and pass 2 fixed the *representation* around
it (continuous two-layer stamping, a decaying ghost of the previous strike,
an anamorphic flash on the strike frame). The day the evaluator lands, this
authoring migrates unchanged; the migration note stays in the header.

## 3. `POST.COMPOSITE` `glow_persist` mode — the machine's smear

The owner's signature ask is the smear (*"like a broken frame buffer"*).
The reel ships **stamp-trail ghosts** today (route 2: re-evaluating emitter
positions at earlier ticks — deliberately steppy, ~10% of a pass per
conduit). The machine's answer is cheaper and better scoped:

**Ask: a `glow_persist` mode of `POST.COMPOSITE`'s glow path, sibling to
`radial_decay`.**

* `glow[t] = sat(gather[t] + k · glow[t-1])`, `k` one frozen Q0.16 decay
  constant on the `61/64` model `radial_decay` already uses.
* **Pass-3 amendment (Direction 3 as amended: "never clears is too much,
  but longer than usual in games. A bit glitchy"):** the decay constant is
  an OWNER KNOB, not frozen at one value, and the mode carries an optional
  **quantised-decay step** — the multiply-by-`k` applied only every N
  frames (N a small register, 1 = every frame) so the trail stutters down
  in visible steps instead of fading smoothly. A smooth exponential fade
  reads as an ordinary motion trail and misses the owner's read entirely;
  the reel's shipped emulation (pass 3: a 96x60 persistence plane with
  kSmearKeepPm / kSmearStepFrames / kSmearJitterPm / kSmearHardClearFrames)
  is the behavioural reference. The per-cell retention jitter and the
  staggered hard clear are reel-side seasoning; if the block wants them
  they are one LFSR against the cell index. Spec text only — still costs
  nothing until the block is built.
* One additional 96x60 RGB565 plane: **+11.5 KB M10K** (doubling the
  existing POSTBUF allocation).
* Cost: 5,760 cells × (read + mul + add + write) ≈ **0.25 of one
  full-screen pass for the ENTIRE frame, at any conduit count**.
* It smears only emissive-tagged content — the mana ghosts; terrain,
  pigment and sky are untouched. The scoping falls out of the existing
  GLOW tag lane for free.
* `POST.COMPOSITE` is phase 11, maturity SPECIFIED, no RTL — this lands as
  **spec text now** and costs nothing until the block is built.

When it lands, the reel's stamp-trail line collapses from ~30.6% of a pass
(three conduits) to +0.25% total.

## 4. Explicitly NOT asked

* **POST.ECHO revival / full-framebuffer feedback.** Cut-order 1, echo-out
  only, ten of thirteen contract sections deliberately unwritten. Building
  the creature's signature look on the first block to be cut is the wrong
  bet; the glow plane buys most of the look at 1/16 the pixels. If
  full-scene RGB persistence is ever wanted, it must be asked by name.
* **A raise of the four-light budget.** Resolved art-side: creature 02's
  showcase clips drop their per-clip sun and run the four-source moving rig
  (R6), so sun + 4 never coexists.

## 5. Cost, assembled (arithmetic, not measurement)

Unit: one full-screen pass = 92,160 pixel-visits at 384x240. No fragment,
particle or polygon counter exists (`spec/counters.md`); every figure below
is arithmetic against stated capacity and must be re-labelled as such
wherever quoted. Deciding case: three conduits, likely shipping stack
(caged pulsar + bullets + lightning + stamp smear):

| element | per conduit | ×3 |
|---|---:|---:|
| outer halo (46–90 px breathing) | ~6–14% | ~19–42% |
| ring core 13 px | 0.7% | 2.1% |
| bolt, continuous two-layer | ~1.5% | ~4.5% |
| 10 plasma bullets @ 8 px | 2.2% | 6.6% |
| stamp-trail smear (3 ghosts/bullet) | ~6.5% | ~19.5% |
| **total, route-2 smear** | **~20–25%** | **~55–75%** |

≈ 3–4% of a frame's clock budget at the placeholder 100 MHz (one pass ≈
5.5% of clocks). With `glow_persist` built, the smear line becomes +0.25%
for the whole frame. Mesh cost is a non-issue (1,45x tris; fill governs).
Bones are now 11 × 8 B = 88 B/frame. The real fill risk remains DDR and the
per-pixel path (`PER_PIXEL_BUDGET.md`: "no slack anywhere") — which is
another reason route-1 smear is the machine's answer.

## Amendment (pass 4, 2026-09-05): `glow_persist` needs a persisted per-cell DEPTH

The owner has rejected draw-on-top persistence ("the smear needs to be
properly hidden whenever the creature is in front of it"). The reference
implementation now keeps a second quarter-res plane holding ONE depth value
per cell — the nearest (largest 1/w) contributing splat depth, written at
feed time, cleared with the cell's hard clear, untouched by decay — and the
composite skips any pixel whose surface depth is nearer than the cell's
remembered depth (exactly the splat path's own test, at cell granularity).

The hardware ask inherits the same contract: `glow_persist` must either
persist a per-cell depth alongside the colour plane (quarter-res: 96x60
cells at 384x240, one depth word per cell), or document FEED-TIME occlusion
only — which cannot occlude a trail after the creature moves in front of
it, and therefore does not meet the owner's ruling. Spec text only; costs
nothing until POST.COMPOSITE is built.
