# Composed fit — the four EDGEWALK commits, measured at last — 2026-09-04

**97.28 MHz. 0.28 ns from the 100 MHz target, and 18 failing endpoints.**

    commit 18054414    device 5CSEBA6U23I7 (provisional, NOT board truth)
    6,493 s wall, fitter 1:30:42 elapsed / 8:46:21 CPU

| | r0 | r9 (`b3bd69b`) | **now** |
|---|---|---|---|
| **`gpu_clk`** | 53.48 | 85.62 | **97.28 MHz** |
| worst setup | −8.697 | −1.679 | **−0.280 ns** |
| **failing endpoints** | — | 430 | **18** |
| ALMs | 12,569 | 12,707 | 13,031 |
| registers | — | 14,812 | 15,546 |
| DSPs | 16 | 16 | **16** |
| hold | — | — | +0.244 ns, **0 failing** |

**+82% on `gpu_clk` over the whole effort, 97% of the original violation
closed, for +462 ALMs and not one extra DSP across ten rounds.**

## What this measured that nothing had

`renderer-b3bd69b-20260901T090000Z` was the newest composed fit in the tree and
**twelve commits to `fpga/rtl/raster` and `fpga/rtl/geometry` had landed since it
ran** — including the four EDGEWALK changes that answered its own recommendation:

    7f95e592  ROW-B/ROW-C split + explicit balanced popcount   (MHZArchitected step 3)
    05cf5e8d  the area's sign bit no longer picks the multiplier's operands
    8918a8f2  the tile-start pixel centre gets registers
    15828e71  the twelve w-operands are job-invariant, so they get registers

The docket had been describing that work as pending for three days. It was
written; it had never been fitted. **This run is the difference between 85.62 and
97.28 MHz, and it cost nothing to obtain but running it.**

## Where the last 0.28 ns is, precisely

    from  ...zhao_raster_fragment:u_fragment | s3_addr_r[3]~DUPLICATE
    to    ...zhao_raster_earlyz:u_earlyz     | acc_mask_r[115]        -0.280 ns

**17 of the 18 violations end inside `zhao_raster_earlyz`**; the eighteenth is
in `zhao_cmd_dma`. Every one is a FRAGMENT → EARLY-Z crossing into the coverage
accumulator mask.

That is worth stating plainly because Early-Z was supposed to be *finished*.
Round 9 replaced its 256-input `acc_full` reduction with unique-coverage
counting and took it from 100 of 100 worst paths to 19. What remains is not that
structure returning — it is the **fragment's stage-3 address arriving late at the
mask**, which is the seam between two blocks that were each optimised alone.

`s3_addr_r` is a register from the RMW split (`fc6395fd`, round 3). So the
remaining path is between two of the fixes, not inside either.

## What is NOT claimed

* **Provisional.** `5CSEBA6U23I7` is a capacity/timing target, not frozen board
  truth. All harness I/O is virtual — no package pins, no board delays, no PLLs.
* **This is the shell WITHOUT the geometry front end.** `zhao_shell_top`
  instantiates one of the twenty blocks in `fpga/rtl/geometry`, and the console
  renders from screen-space triangles handed in from outside (docket D22). The
  number is honest for what it measured and it is not the finished console's
  number.
* `gpu_clk` and `vid_clk` remain timing-related, so the known phase-dependent
  displayed-byte crossing is not waived.

## The next step, and it is one path

The shortfall is 0.28 ns on a single cross-block seam with seventeen siblings.
That is a smaller and more specific problem than any round in this effort has
started with, and it does not need another sweep: it needs a register between
`zhao_raster_fragment`'s stage-3 address and `zhao_raster_earlyz`'s mask, or the
mask write moved a stage later.

**Architecture rule still applies:** latency may grow; initiation rate and exact
arithmetic may not regress.
