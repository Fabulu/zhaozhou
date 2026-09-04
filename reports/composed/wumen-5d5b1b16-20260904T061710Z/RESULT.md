# Composed fit — **100 MHz, ZERO failing endpoints. D1 is closed.** — 2026-09-04

**`timingPassed: true`.** The first time this has ever been true.

    commit 5d5b1b16    device 5CSEBA6U23I7 (provisional, NOT board truth)
    5,989 s wall

| | r9 `b3bd69b` | r10 `18054414` | r11 `3546bfa2` | **r12, this run** |
|---|---|---|---|---|
| **`gpu_clk`** | 85.62 | 97.28 | 98.06 | **100.00 MHz (target met)** |
| worst setup | −1.679 | −0.280 | −0.198 | **+0.057 ns** |
| **failing endpoints** | 430 | 18 | 12 | **0** |
| hold | — | +0.244 | — | **+0.245, 0 failing** |
| ALMs | 12,707 | 13,031 | 13,301 | 13,413 |
| registers | 14,812 | 15,546 | — | 15,937 |
| DSPs | 16 | 16 | 16 | **16** |

**+87% on `gpu_clk` across the whole effort, the entire original violation
closed, for +844 ALMs and not one extra DSP over eleven rounds.**

## One change did it, and it was not the one the previous round recommended

The only RTL change to the raster path since `3546bfa2` is
`zhao_raster_tilestore.sv` indexing its present bit by **port** address rather
than by **bank** address:

    res_pres_eff = (front_r == 1'b0) ? present1[res_addr_i] : present0[res_addr_i];
    rd_pres_eff  = ... ? present0[rd_addr_i] : present1[rd_addr_i];

Equivalent by case analysis — `present1[b1_raddr]` is only ever evaluated under
`front_r == 0`, where `b1_raddr` **is** `res_addr_i`. The bank mux was carrying
`rd_addr_i` into a lookup that can never select it, so RASTER.FRAGMENT's retire
chain reached `res_pres_q` down a path **unreachable in the design's own
semantics**.

## Twelve violations closed by one fix, and that is the flat tail paying off

The previous round found four unrelated offenders **all inside 0.2 ns** and drew
the conclusion that *"closing the last 0.198 ns needs all four touched, or the
target reconsidered."* **That was wrong, and pleasantly so.**

Removing the worst path did not merely remove one path: it freed the fitter
across the whole region, and the other three structures — `mem_guard` ×6,
`binner` ×4, `cmd_dma` ×1 — closed on placement alone. **A flat tail cuts both
ways**: no single fix buys much on its own, but the last structural offender
removed can let everything else settle at once.

The `binner` rework costed in the previous round — six speculative adders and
six `fill` predicates to preserve the initiation rate — **was never needed.**
Deferring it on the grounds that an unmeasured restructure can cost 2 MHz was
the right call for the right reason.

## The new worst paths are ordinary

    +0.057  edgewalk | pend_r[8]        -> (raster)
    +0.060  cmd_dma  | m.M_HCRC         -> crc_pay_r[21]
    +0.082  fragment | s0_addr_r[0]     -> (raster)

Three different structures, all positive, none dominant. There is no next
surgery indicated.

## What is NOT claimed, and the margin is thin

* **Provisional.** `5CSEBA6U23I7` is a capacity/timing target, not board truth.
  All harness I/O is virtual — no package pins, no board delays, no PLLs.
* **+0.057 ns is 0.57% of the period.** It is a pass, not comfort. A real
  device, real pins and a real PLL will all spend some of it.
* **This is the shell WITHOUT the geometry front end** (docket D22).
  `zhao_shell_top` instantiates one of the twenty blocks in `fpga/rtl/geometry`
  and renders from screen-space triangles handed in from outside. **The number
  is honest for what it measured and it is not the finished console's number.**
  D22's two blockers were both cleared today, so what remains there is wiring —
  and wiring nineteen blocks into a shell with 0.057 ns of margin will move
  this number.
* `gpu_clk` and `vid_clk` remain timing-related, so the known phase-dependent
  displayed-byte crossing is not waived. 5 critical warnings, all the virtual
  clock-port class.

## The rule this effort kept re-learning

Three of the four offenders in round 11 were first diagnosed from module names
and RTL reading, and **three of those diagnoses were wrong in the part that
picks the fix.** The one that closed the design was found only by reading
`characterization/setup_paths.rpt` node by node — where it turned out that 6.3
of 9.58 ns were a 256:1 multiplexer nobody had suspected, fed by an address that
could not occur.

**A trace is not traced until it names nodes.**
