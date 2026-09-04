# Composed fit — **D1 REOPENS at −0.066 ns**, and the cause is correctness — 2026-09-04

**`timingPassed: false`.** Three failing endpoints, worst setup **−0.066 ns**.

    commit e6f31aa6    device 5CSEBA6U23I7 (provisional, NOT board truth)

| | r12 `5d5b1b16` | **r13, this run** |
|---|---|---|
| worst setup | **+0.057** | **−0.066 ns** |
| **failing endpoints** | **0** | **3** |
| hold | +0.245 | +0.252 ns, 0 failing |
| ALMs | 13,301 | 13,421 |
| registers | 15,937 | 15,971 |
| DSPs | 16 | **16** |

## What changed, and it was not optional

One RTL change since r12: `zhao_raster_fbwrite` gained a `W_VERD` state, because
it had been reading `zhao_mem_guard`'s verdict **one cycle early** and therefore
**could not write a single framebuffer row**. Before the fix the composed shell
rendered `pixels=0`; after it, 3,328 — the exact extent `zref::Binner` predicts.

**The console could not draw at 100 MHz. It can draw at 99.3.**

## The three offenders are NOT in the block that changed

    -0.066  tile_pipe | job_first_r     -> tile_pipe
    -0.042  cmd_dma   | need_total[5]   -> cmd_dma | burst_end[31]
    -0.029  edgewalk  | pend_r[12]      -> (raster)

**`fbwrite` appears in ZERO of the worst 100 paths.** The families that do:
`bin_pipe` 146, `tile_pipe` 106, `edgewalk` 99, `cmd_dma` 45, `binner` 40.

So the fix did not create a slow path. It added 34 registers and 8 ALMs, the
fitter placed differently, and three paths that were already within a hair of
the line went over it. **r12's own RESULT.md predicted exactly this**: *"no
dominant offender left ... the deepest violation is 0.065 ns worse than the
shallowest."*

## THE REAL FINDING: +0.057 ns was never a margin

r12 closed at **0.57% of the period**. This run spends 0.12% of it on a
mandatory correctness fix and the design fails. **A pass that thin does not
survive the next necessary change**, and there will be more of them — D22 alone
adds nineteen blocks.

r12's RESULT.md called +0.057 "a pass, not comfort". That was right, and this is
what it looks like when the bill arrives.

## What NOT to do

**Do not revert the fbwrite fix.** It buys 0.066 ns and a console that renders
nothing. Timing on a machine that cannot draw is not a result.

## What the next round is

The offender list is the same flat tail as r12, one place further along, and it
is now genuinely spread across four families. Options, in the order they should
be tried:

1. **`tile_pipe | job_first_r`** at −0.066 is the only one that needs to move to
   pass. One path, one structure.
2. **`cmd_dma`** at −0.042 has appeared in every round since r10 and has never
   been touched.
3. **`edgewalk | pend_r`** at −0.029 is the shallowest and may go on placement
   alone once either of the others moves.

**Or reconsider the target.** 99.34 MHz on a provisional device with virtual I/O
is not obviously the wrong number to build a console around; 100 was chosen, not
derived. That is an owner call and is recorded here rather than assumed.

## What is NOT claimed

* **Provisional.** `5CSEBA6U23I7` is a capacity/timing target, not board truth.
* The render path this measures is now **simulated as well as fitted**
  (`shell_draw_directed`, 20 checks) — which was not true of any previous round.
