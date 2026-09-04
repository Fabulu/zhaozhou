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

**IT IS A READY PATH, NOT A DATA PATH**, and that distinction decides the fix.
Traced through the RTL rather than inferred from the endpoint names:

    fragment.s3_addr_r
      -> (s3_addr_r == s0_addr_r)   the same-address hazard comparator
      -> s0_to_s1 -> fragment.frag_ready_o
      -> earlyz.cand_ready_i
      -> out_free = !out_v_r || cand_ready_i
      -> earlyz.frag_ready_o -> frag_acc -> hiz_qualify
      -> acc_mask_r[frag_addr_i] write enable

**A combinational backpressure chain spanning two blocks and ending in a 256-bit
masked write.** `s3_addr_r` reaches Early-Z by travelling *backwards* through
the ready signals, not forwards as data.

The first version of this file said "the fragment's stage-3 address arriving
late at the mask", which reads as a data path. **It is corrected here because
the two have opposite fixes**: pipelining the address would do nothing at all,
since the address is not what arrives.

Early-Z was supposed to be finished — round 9 replaced its 256-input `acc_full`
reduction with unique-coverage counting and took it from 100 of 100 worst paths
to 19. This is not that structure returning. It is the seam between two blocks
each optimised alone, and `s3_addr_r` is a register from the RMW split
(`fc6395fd`, round 3), so the path runs *between* two of the fixes.

**And this class has a regression history.** `MHZArchitected` round 2 added an
Early-Z ready-path skid: `gpu_clk` fell 62.89 -> 60.92 and it was KEPT anyway as
prepaid work. Whoever breaks this chain should expect the same shape and fit it
alone.

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
started with, and it does not need another sweep: it needs the READY chain broken -- a skid on
`earlyz.cand_ready_i`, or the hazard comparator taken off
`fragment.frag_ready_o` -- and it needs its own fit, because the last attempt at
this class cost 2 MHz before it paid.

**Architecture rule still applies:** latency may grow; initiation rate and exact
arithmetic may not regress.
