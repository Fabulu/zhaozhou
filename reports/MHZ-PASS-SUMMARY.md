# The 100 MHz pass: where it got to, and what the numbers mean

Written 2026-09-02, after sixteen fits and three seed-variance measurements.
Read `NOISE_FLOOR.md` first if you read only one thing — it changes how every
number below should be interpreted.

## The result

| | |
| --- | --- |
| **Start** | 53.48 MHz, -8.7 ns worst slack |
| **Best measured** | **96.73 MHz** (seed 3) |
| **Typical** | **94.8 MHz, sd 1.7** over three seeds (94.21 / 93.43 / 96.73) |
| **Target** | 100 MHz |
| **Cost** | +287 ALMs, 0.6% of the device |
| **DSP** | 16, unchanged across all sixteen fits |
| **Correctness** | every gate green; all pixel CRCs bit-identical |

Roughly **+81%**, closing about 92% of the original violation, without spending
DSPs and without a single bit of output changing.

## What was actually wrong, in the order it was found

Every one of these was found by a fit naming a path. **Not one was found by
reading the RTL** — including the four in the block already rewritten three
times.

| # | block | the fault |
| --- | --- | --- |
| 1 | FRAGMENT | read-modify-write loop closed in one cycle |
| 2 | RESOLVE | RAM-launched combinational path (M10K clock-to-out ~2 ns) |
| 3 | EARLY-Z | 256-input AND whose result fanned back to all 256 inputs |
| 4 | EDGEWALK | the area's SIGN BIT steering a DSP's operands in the DSP's own cycle |
| 5 | EDGEWALK | per-pixel steps recomputed every row from job constants |
| 6 | EDGEWALK | tile pixel centre recomputed every cycle from a job constant |
| 7 | EDGEWALK | twelve multiplier operands recomputed from job constants |
| 8 | SCANOUT | `fetch_line * 768` recomputed every cycle; it changes once a line |
| 9 | ARBITER | burst length computed AFTER the arbitration that selects it |

**Six of nine are one mistake**: arithmetic on a value that is stable for the
whole operation, left combinational because it looks too cheap to bother
registering, standing at the head of a long path. It is invisible when reading —
a shift, an add, a subtract — and it is the single most productive thing to look
for in this codebase.

The other three are the same shape one level up: **something waiting for a
decision it does not depend on.** The floor compare waiting on the min compare,
the burst length waiting on the arbiter, the multiplier operands waiting on the
winding flip. When a select feeds arithmetic, computing every branch and
selecting the RESULT is shorter than selecting the operand and then computing.

## Bro's five named offenders, final accounting

    FRAGMENT   fixed twice, the largest single contributor
    EDGEWALK   fixed FOUR times, a different tail each time
    EARLY-Z    fixed twice
    BINNER     absent from every worst-100 for eleven fits; appeared at ~95 MHz
    FBWRITE    never appeared at all, in sixteen fits

Two of five were named from reading the RTL and never confirmed by a
measurement. BINNER's late appearance is not vindication: what changed is the
SLACK, not the block. A path that is comfortable at 53 MHz and marginal at 95
was not an offender at 53.

## The measurement problems, which cost more than the RTL problems

Four separate times a number that LOOKED like evidence was measuring something
else, and each was believed **because a tool produced it**:

1. **Launch registers classified by NAME.** Quartus packs `cross_r` into a DSP;
   the tool called it a fabric flop. Reported `fabric_ff=200` when the truth was
   52 fabric / 48 DSP — and would have shown *no change* across the most
   decisive fix of the pass. Bro caught this.
2. **A noise floor never measured.** "~1.5 MHz", quoted for fifteen rounds,
   extrapolated from `audio_clk` on a differently constrained domain. Measured:
   **4.61 MHz**.
3. **Owners ranked by path COUNT.** Early-Z owned 78 of 100 paths and was not
   the limiter; `vram_arbiter` owned four and set Fmax.
4. **Endpoint counts read as structural.** "291 → 6 endpoints" compared two
   placements, not two designs — the same commit shows 291 at one seed and 43 at
   another.

The generalisation, now the standing rule: **every aggregate the STA report
produces is a property of the placement.** Worst slack, endpoint count, owner
table, even which blocks appear at all. Any of them can move fiftyfold between
seeds on identical RTL.

Placement-independent, and therefore what to argue from:

* a structural count with a stated mechanism (`DSP-launched 48 -> 0`, because a
  named edit provably removed `cross_r[47]` from the multiplier's cone)
* bit-exactness: CRCs, oracle comparisons, formal proofs
* differences large enough to dwarf the band: 53.48 -> ~96

## What the last 4 MHz needs, and why it is not another round of this

**Three seeds produced three different limiter blocks.** Same commit, same
sources, only the placement seed differing:

    seed 1   94.21 MHz   zhao_cmd_dma          -0.615  (40 paths)
    seed 2   93.43 MHz   zhao_raster_tilestore -0.703  ( 3 paths)
    seed 3   96.73 MHz   zhao_raster_earlyz    -0.338  (41 paths)

No single block limits this design. That is the signature of many paths sitting
at similar slack with no dominant structure, and it settles the question: local
surgery now has low and largely unmeasurable expected value. Each candidate
costs two to three fits (3-5 hours) to distinguish from noise, the block it
targets may not be the limiter at another placement, and the honest answer will
usually be "inconclusive".

**Round 16 is the worked example.** Three seeds before the arbiter fix and three
after:

    pre-arbiter   91.31 / 95.70 / 95.92   mean 94.31   sd 2.60
    round 16      94.21 / 93.43 / 96.73   mean 94.79   sd 1.72

A +0.48 MHz difference of means against sd ~2.2 with n=3. **Not resolvable.**
The change is kept because it is architecturally right — arithmetic that waited
on an arbitration no longer does — and not because the numbers show a gain. They
do not.

The remaining structural targets are real, and each costs a cycle somewhere:

* **EARLY-Z's 256:1 presence lookup**, fed by a priority-encoded address from
  tile_pipe — about 2.6 ns of encode-plus-mux. Breaking it needs a pipeline
  stage at the module boundary, which the reference model REJECTED when it was
  tried in round 12: 8 decisions diverged, because `zref::EarlyZ` promotes the
  floor in the same cycle and the next fragment sees it.
* **TILESTORE's presence mux**, the same shape.

Both are read-modify-write structures over 256 entries where the address arrives
late and the lookup is inherently a mux tree. They belong to the spec's sections
5 and 8-13 — top-level islands and clocking — not to another constant-hoisting
round.

## Two debts, both quantified

* **S_W0B costs one clock per tile job**: +2.6% on a full tile, +3.7% on a
  sliver, MEASURED by `raster_edgewalk_setupcost` (setup 6 -> 7 clocks), not
  estimated. The repayment is designed and unapplied: apply the winding flip to
  the subtraction ORDER after the multiply rather than to the operands before
  it, which keeps `cross_r[47]` off the DSP path at zero cycle cost.
* **The clean-HEAD snapshot costs ~40 minutes per fit** (~250 MB, ~3300 files
  extracted). Archiving only `fpga/`, `tests/CMakeLists.txt` and
  `tools/quartus/` would cut it to a few MB. Deliberately not done mid-series;
  do it between passes, then re-fit one unchanged commit to prove the number did
  not move.
