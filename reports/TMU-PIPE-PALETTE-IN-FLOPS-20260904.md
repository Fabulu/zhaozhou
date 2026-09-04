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

---

# The fix, designed from every access site (not from the sizes)

Written while the fit was still routing, by reading all fourteen references to
the three palette arrays. **Only ONE of them should move to memory**, and a
naive "put the palette in RAM" would break the block.

## `pal_dat_r [PAL_SLOTS][256]` of 16 bits — 65,536 bits — MOVE IT

Every access is a single address per cycle:

    read   dec_clut565_c = decode16(pal_dat_r[pal_way_c][rsp_idx], FMT_RGB565);
    write  pal_dat_r[pal_way_c][rb_idx[rsp_rec]] <= cac_data_i[15:0];
    write  pal_dat_r[pal_vic_r][rb_idx[rsp_rec]] <= cac_data_i[15:0];

One read port, one write port, both fully addressed. That is a textbook **1R1W
memory**. Flattening the two dimensions into one array indexed by
`{way, entry}` — 4,096 entries x 16 bits — is inferrable, and 65,536 bits is
**7 M10K out of 553 available**.

The two writes are mutually exclusive branches of the same `if`, so they remain
one write port after flattening.

### CORRECTION, before this was applied: flattening ALONE will not work

The paragraph that stood here said the read address is already available a cycle
early "so this does not add a stage". **That was wrong, and it was wrong in the
way this repository keeps writing down**: I checked the access sites and did not
check the one property that decides whether a memory can exist at all.

**The read is COMBINATIONAL.** Line 557 opens an `always_comb`, and line 559 is

    dec_clut565_c = decode16(pal_dat_r[pal_way_c][rsp_idx], FMT_RGB565);

An M10K cannot be read asynchronously. Block RAM inference requires the read
data to come out of a register, so a flattened `pal_dat_r[{way, idx}]` read in
`always_comb` **stays in flip-flops** — Quartus will do exactly what it is doing
now, and the 65,536 registers will not move. Flattening is necessary and not
sufficient.

The real fix therefore has two parts, and the second one costs something:

1. flatten to `[4096]` indexed by `{way, entry}` — removes the multidimensional
   shape;
2. **register the read**, which puts `dec_clut565_c` one cycle later than
   `dec_pal565_c` and `dec_direct_c` beside it, so the consumer's mux and the
   retire path have to move with it.

### And the cost lands on the path that is already the tightest

The block's own directed suite prints, on a green run:

    the demand-critical path is CLUT (terrain is CLUT8) at 0.65x the 850,000
    demand and 0.67x the 829,440 it was rounded up from

**The CLUT path is the palette path** — the one this fix touches — and it is
already at 0.65x of demand. Adding a cycle to it without absorbing that cycle
elsewhere makes a known shortfall worse. Whether 65,536 registers or CLUT
throughput matters more is an owner call, not a tidy-up, and it is now a
genuine trade rather than the free win the first draft described.

**What does not change:** the block currently spends about 87% of the device's
registers on this array, which is not survivable either. The choice is between
paying a pipeline stage and paying the registers — not between fixing it and
leaving it.

## `pal_val_r [PAL_SLOTS]` of 256 bits — 4,096 bits — LEAVE IT IN FLOPS

This is the one that makes the naive fix wrong:

    for (int unsigned w = 0; w < PAL_SLOTS; w++)
      if (pal_ten_r[w] && (pal_tag_r[w] == rb_pal[rsp_rec]))
        pal_ent_c = pal_val_r[w][rsp_idx];

`pal_val_r` is read on **all sixteen ways in the same cycle** — it is the
associative half of a 16-way lookup — and it is cleared sixteen-ways-at-once by
`pal_val_r[w] <= 256'd0`. A memory cannot do either. **4,096 bits in flip-flops
is the CORRECT implementation for that access pattern**, not a defect that
happens to be smaller.

## `pal_tag_r [PAL_SLOTS]` of 32 bits — 512 bits — LEAVE IT IN FLOPS

Same reason: compared on all sixteen ways simultaneously in the loop above.

## Expected result

    registers   72,824  ->  about 7,300     (-90%)
    M10K             2  ->  about 9
    block memory   256  ->  about 65,800 bits

The remaining ~7,300 registers are the ROB, the pipeline, and the 4,608 bits of
tag/valid state that are correctly flops.

## Why this is written down before it is applied

`zhao_texture_tmu_pipe.sv` is inside the RUNNING fit's source closure, and the
fit reads the working tree (`QUARTUS_GOTCHAS.md` §11). Editing it now would
corrupt a measurement that has already cost 45 minutes of placement
preparation and 25 of placement.

It is also written down because **the design is the part worth reviewing, and
it came from reading access sites rather than from the sizes.** The sizes say
"69,632 bits are in the wrong place"; only the access patterns say *which*
69,632 — and they say 65,536 of them, with the other 4,608 staying exactly where
they are. A fix driven by the byte count alone would have moved all three arrays
and broken the associative lookup, which is the kind of error that passes a
resource gate and fails a picture.

---

# Where the registered read should actually be built: with the II = 2 work

The correction above leaves a trade — pay a pipeline stage on the CLUT path, or
pay 87% of the device's registers. **There is a third option, and it is already
designed and written down by somebody else.**

`reports/REMAINING_BLOCKERS.md` records the CLUT throughput shortfall and its
fix:

> **6 clocks per CLUT sample = 277,778/frame against 850,000.** Terrain is
> CLUT8, so that is the demand-critical figure. The fix is designed and written
> into `design/contracts/TEXTURE.TMU.md` (II = 2 needs a 2-entry in-flight
> record, an issue arbiter over the single cache port, and in-order completion;
> II = 2 is the port's own floor because a CLUT sample needs two serial
> accesses). **Not built.**

That design **already** turns the CLUT path from a serial six-cycle walk into a
pipeline with a 2-entry in-flight record and in-order completion. A registered
palette read is a pipeline stage — and a pipeline with in-flight records is
precisely the structure that absorbs one without adding to the sample interval.

So the two items should be built together:

* **alone**, the D19m fix adds a seventh cycle to a six-cycle serial path that
  is already at 0.65x demand — it trades a resource problem for a throughput
  problem;
* **with II = 2**, the registered read lands inside a restructure that was going
  to touch the same cone anyway, and the II the fix is aiming for is set by the
  two serial cache accesses, not by the palette lookup.

**Neither of these was found by looking at the other.** D19m came from a fit log
and D19l from a JSON sweep; the II = 2 design has been sitting in the blockers
report since 2026-08-23. What connected them was running the block's existing
directed suite for a baseline and reading the line it prints on a green run.

**This is a recommendation about sequencing, not a decision.** Building II = 2
is a larger job than reshaping an array, and whether the register cost is
tolerable until then is the owner's call. The point is that doing D19m *first
and separately* is the one ordering that makes both problems worse.
