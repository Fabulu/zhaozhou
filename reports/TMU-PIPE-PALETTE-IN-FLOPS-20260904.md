# tmu_pipe holds a 64 Kbit palette cache in flip-flops

Measured 2026-09-04 from the LIVE fit's synthesis stage, before the fitter
finished routing — the numbers below are Analysis & Synthesis, which completed
in 10 minutes and was already on disk while placement ran for another 71.

    Top-level Entity Name    : zhao_texture_tmu_pipe
    Total registers          : 72824
    Total block memory bits  : 256
    Total DSP Blocks         : 3
    Analysis & Synthesis      : Successful, 0 errors, 109,800 warnings

**72,824 registers.** The provisional device (5CSEBA6U23I7) has 41,910 ALMs, so
roughly 84,000 flip-flops in total: **one block is asking for about 87% of every
register on the part**, while using 256 bits of the 553 available M10Ks.

## The cause is two declarations

    parameter int unsigned PAL_SLOTS = 16;

    logic [255:0] pal_val_r [PAL_SLOTS];        //  16 x 256   =  4,096 bits
    logic [15:0]  pal_dat_r [PAL_SLOTS][256];   //  16 x 256 x 16 = 65,536 bits

69,632 bits of palette cache, which at one flop per bit is 69,632 registers —
essentially the entire count. `pal_dat_r` is **multidimensional**, which is the
exact shape Quartus 17.0.2 declines to infer as RAM, reporting *"cannot regroup
multidimensional array"*. As memory it would be **7 M10K blocks out of 553**.

## Why nothing caught it

`design/fit_targets.yml` gives `zhao_texture_tmu_pipe` a `sources:` list and
**no `rules:` at all**. The block with the worst storage shape in the tree is
the one block with no tripwire on storage shape.

Compare `zhao_texture_fragrob`, which has `max_registers: 2500` and failed it at
2,631 — a 5% overrun that stopped the block, correctly. tmu_pipe is over by a
factor of roughly thirty and passes, because nobody wrote the line.

**A gate absent is not a gate passing**, and a report that lists tmu_pipe beside
fragrob without saying which of them was actually checked invites exactly the
wrong reading.

## What this does NOT say

* **Not that the fit will fail.** It may well place and route; it was still
  routing when this was written, and placement already succeeded. Fitting is not
  the question — a block that consumes 87% of the device's registers has failed
  a budget long before it fails a fitter.
* **Not that the RTL is wrong.** A palette cache of 16 slots x 256 entries is a
  reasonable thing to want. Holding it in flops is the defect, not holding it.
* **Not measured by me at the shipping parameters of the whole island.** This is
  one leaf compiled alone. Per-block fits do not share, so this does not compose
  with the other eight into a device total.

## The prediction that failed this morning, and why this one is different

Earlier today I predicted `fragrob` would fail on exactly this mechanism —
`desc_u_m[3][DEPTH]` being multidimensional and therefore uninferrable. **It did
not**: fragrob inferred 13 M10Ks and its multidimensional array was fine. I
recorded that the blocker "is real for some shapes and was not fragrob's".

This is one of the shapes. The difference is scale and indexing: fragrob's
arrays are small and read whole, while `pal_dat_r [slot][entry]` is a genuine
two-level random access of 64 Kbit. Being multidimensional is not sufficient to
break inference; being multidimensional AND large AND indexed on both axes is
what does it.

## Next step, not taken here

The fix is a shape change — one M10K-inferrable array indexed by
`{slot, entry}`, or an explicit RAM primitive — plus a `max_registers` rule so
the tree cannot lose this again. **Neither is done in this pass**, because
`zhao_texture_tmu_pipe.sv` is inside the RUNNING fit's source closure and
editing a file a live fit is reading is the live-tree trap (`QUARTUS_GOTCHAS.md`
§11). It waits for the fit to finish, which is also when the routed numbers
arrive to confirm or refute the register count above.
