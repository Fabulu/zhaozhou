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

---

## Amendment (pass 6 recon, 2026-09-05): the ROW TEAR is required, and the "seasoning" is the feature

**Ask 3 currently demotes the per-cell jitter and the staggered hard clear to
optional reel-side seasoning — *"if the block wants them"*. That is now wrong,
and the row tear is missing from the ask entirely.**

The owner has since seen the shipped clips and picked the smear he wants **by
name**, from `manafold-hasty`:

> "That one also has the perfect glitchy frame buffer looking thing the mana does
> that leaves this weird trail behind. We want that. Makes our mana look unique."

Engine recon then established what he actually pointed at. `hasty` runs
`kSmearPresets` rung **3** — `{keep 900, step 6, jitter 160, hard_clear 430,
gain 520, tear 1}`. `channel` runs rung **1**, with **`tear = 0`**. The
distinguishing ingredient between the rung the owner called perfect and the one
he did not is **`kSmearTear`**: 5 rows, 46 frames, ±2 cells of horizontal row
displacement.

**So the row tear is not a garnish on the smear. It is the thing that makes the
smear read as a BROKEN FRAME BUFFER rather than as a motion blur.** A clean
exponential trail — even a stuttering one — is a trail. Rows sliding sideways by
a couple of cells is what makes it look like hardware failing, which is the
entire aesthetic the owner asked for twice and has now confirmed on screen.

**Revised ask, superseding the "if the block wants them" clause:**

1. **Per-cell retention jitter** — REQUIRED, not optional. One LFSR against the
   cell index, as already described.
2. **Staggered hard clear** — REQUIRED, same reason.
3. **Row tear** — NEW, and the most important of the three. A small number of
   scanline rows (reel reference: 5) displaced horizontally by a small signed
   cell offset (reel reference: ±2 cells), re-rolled on a slow period (reel
   reference: 46 frames). All three want to be owner knobs, per the standing
   rule that every shape, colour and timing value belongs in a named, editable
   constant.

Cost note: a row tear is an addressing offset on the persistence plane's read,
not extra storage or extra arithmetic — it should be close to free in the block,
and it is the highest-value single element in the whole smear.

**Do not implement `glow_persist` as decay-plus-step only.** That was the pass-3
reading, and it predates the owner choosing a rung. A block that ships smooth or
merely-stuttering decay will not produce the look that has now been approved on
screen, and the reel will keep emulating the tear in software at ~10% of a pass
per conduit.

**Evidence:** `runs/CLAUDE-RUNS/RUN-20260905-1910-manafold-p6-recon-engine/`
(`FINDINGS.md`, plus `evidence/hasty-smear-zoom.png` — the 4x before-plate
showing the chunky pale-cyan residue with its hard 4-px edges).
