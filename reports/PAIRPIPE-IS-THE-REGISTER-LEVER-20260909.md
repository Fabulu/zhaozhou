# The pair-pipe is not a DSP lever. It is the register lever — and the like-for-like map to prove it was already on disk

2026-09-09. Completes `PAIRPIPE-IS-NOT-A-DSP-LEVER-20260909.md`, whose last line
says the comparison it needed **"is running now"**. It finished, and nobody read
it against the pair-pipe. Zero new compute was needed for anything below.

## The frame was wrong, for the second time today

`zhao_raster_perspuv_pairpipe` was assessed against **DSP**, correctly found not
to help — it keeps both multipliers by design, because *"the two axes compute
different numerators against a shared mantissa and saturate independently…
serializing them would halve the throughput to buy nothing"* — and came off the
lever list.

Meanwhile the criterion it *does* move is the one nobody was measuring. The
island misses its register budget by **+81%**, worse than its ALM miss of +44%,
and `zhao_raster_perspuv_svc` is the largest single register consumer in it.

This is the same shape as `rcp24_v3`, judged as an 8%-of-ALM lever and dismissed
while being decisive for DSP. **Twice in one day a block was dismissed against the
criterion it does not help.** The lesson is not "check DSP and registers"; it is
that a lever list organised by *remedy* silently inherits whichever criterion was
urgent the day it was written.

## What the pair-pipe deletes, in its own words

> `zhao_raster_perspuv_svc` carries TWO of everything on the control side: two
> work queues, two write pointers, two read pointers, two emptiness tests, two
> token selects. It then reassembles the axes at the far end through a
> sixteen-entry table with a per-axis `e_have` join and two result tables
> `e_q_u`/`e_q_v`. **None of that duplication does anything.**
>
> The join disappears because there is nothing left to join: the pair travels as
> ONE item through one pipeline, so `e_have`, `e_q_u`, `e_q_v` and the
> sixteen-entry operand tables have no reason to exist.

Those are exactly the structures that hold `perspuv_svc`'s registers. Its `_q`
pipeline state is only **594 bits — inside its 700-bit budget line**; the
**3,376-bit token table** is 85% of the block. The pair-pipe removes the table
rather than trying to make it inferable.

It is not a proposal. It rests on `PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md`, an
induction proof that the two schedulers are bit-identical for all time, and
`tests/raster/perspuv_lockstep_directed.cpp` asserts that every cycle against the
elaborated RTL, with depth-zero fragments in the mix and a live-probe control so
its zeros mean something.

## The like-for-like comparison, which now exists

The earlier report refused to compare a fit row against a map row, and was right
to: *"'2,255 fewer registers' is sitting right there and it would be wrong."* It
estimated a ~1.7× fit/map replication from two `texture_combine` pairs and
discounted the pair-pipe to *"perhaps ~1,600 fitted"*, calling that **still not a
number I have measured**, and launched `perspuv_svc@map` to fix it.

That row is on disk. Both of these are MapOnly, same tool, same device, both
`rtlCleanAtHead: true`:

| | registers | memory bits | DSP |
|---|---:|---:|---:|
| `zhao_raster_perspuv_svc@map` | **3,361** | 256 | 6 |
| `zhao_raster_perspuv_pairpipe@map` | **961** | **1,280** | 6 |
| delta | **−2,400** | +1,024 | 0 |

**And it corrects the discount.** The assumed ratio does not hold for this block:

```
perspuv_svc   FIT 3,216 registers    MAP 3,361 registers    map/fit = 1.045
texture_combine pairs (the report's basis)                   map/fit ≈ 0.59
```

For `perspuv_svc`, map and fit registers are within 4.5% — fitting does **not**
replicate them the way it does in the combiner pairs. So the earlier ~1,600
estimate rested on a ratio measured on different blocks, and it was too
conservative.

## What is established, and what is still an estimate

**Measured, like-for-like:** the pair-pipe maps to **2,400 fewer registers** and
puts **1,024 more bits in memory** at identical DSP.

**Estimated:** its *fitted* register count, because **the pair-pipe has never been
fitted**. Bracketing with the two ratios this ledger actually contains:

| assumed map/fit | pair-pipe fitted | saving vs `svc`'s fitted 3,216 | share of the 7,285 overage |
|---|---:|---:|---:|
| 1.045 (this block) | ~920 | ~2,296 | **32%** |
| 0.59 (combine pairs) | ~1,629 | ~1,587 | **22%** |

So **22–32% of the island's register overage, from a block that is already built,
already proven, and already tested.** That is by a wide margin the largest single
lever found today — the monotone-chain encoding was ~6%, the ROM packets 618 ALM,
`OWNERS` closed at 32/64, and the RAM-inference route is still hunting a blocker
after six eliminated candidates.

**Not claimed:** that it closes the register gate. 22–32% is not 100%, the
register breach is systemic across 9 of 11 components, and ALM and Fmax are
separate failures.

**Not claimed:** ALM or Fmax effects. Neither map row carries ALMs, and neither
block's Fmax appears in a map. The pair-pipe's header expects an ALM saving too
and that remains unmeasured.

## The one thing that would close it

**One MapOnly is not enough; this needs one FIT of `zhao_raster_perspuv_pairpipe`.**
It is the only way to turn the 22–32% range into a number, and it also returns the
ALM and Fmax columns that no map can. A fit is the owner's call under §0.2 — but of
the fits waiting, this is the one with the clearest question attached: *does
replacing `perspuv_svc` with the pair-pipe remove ~2,300 registers and what does it
cost in ALM and Fmax?*

It is also **not** one of the twelve queued fits, and it is a leaf, not the island
— minutes-to-an-hour, not 1.5–4 hours.

## The loose end this closes, and the habit it argues for

`PAIRPIPE-IS-NOT-A-DSP-LEVER` ended with *"`zhao_raster_perspuv_svc@map` is
running now… it is the difference between a claim and a measurement."* The run
completed and the comparison was never made, so the conservative estimate stood
as the working number for a day while the evidence to sharpen it sat in the
ledger.

**A report that ends by naming a running job needs something that reads the row
when it lands.** That is the same gap as the four `.rgb`-purge lesson: the
half-fix was thorough and the other half was never built.


---

# The swap recipe, written out so a "go" is one reviewed step and not an improvisation

**Not performed.** This is what changing it would take, recorded because step 6 is
a trap this repository has already paid for twice.

`FIT GATE 3` is the repo's own name for the measurement, registered in
`design/fit_targets.yml` and referenced from `design/prod_manifest.yml:334`. It is
**two leaf fits**, not one: the candidate, and a **fresh `perspuv_svc` row from the
same commit**, because the standing svc row predates today's tree and *"comparing
against a stale measurement is the error this file already documents twice."*

## What actually changes

1. **`fpga/rtl/texture/zhao_texture_island_v3_top.sv:974`** — module name only.
   `zhao_raster_perspuv_svc` becomes `zhao_raster_perspuv_pairpipe`; the
   `#(.NTOK(16), .TAGW(16))` list and all eighteen connections are unchanged,
   because the candidate's ports are a strict superset.
2. **`fpga/rtl/texture/zhao_texture_island_top.sv:788`** — the same, for the v1
   island top.
3. **Connect `zero_products_o`.** It is the one new port, and it is a *working
   instrument*: it counts depth-zero fragments that produce no product, and
   `perspuv_pairpipe_directed` asserts it against an independently counted
   `zero_accepts` with `saw_dz > 0` proving it moves. Leaving it unconnected
   silently discards tested coverage — which is how a live counter becomes
   decoration.
4. **`design/prod_manifest.yml`** — move the candidate out of `unused` into `top:`
   and `zhao_raster_perspuv_svc` into `excluded: superseded`. Doing exactly one of
   those two would double-count or under-count the island; the manifest's own
   comment says counting the candidate now "would double-count", so the pair of
   edits is one atomic change.
5. **`design/fit_targets.yml`** — keep BOTH targets. The svc target becomes the
   historical comparison row and its rules are deliberately identical, so the
   comparison stays like-for-like.
6. **Regenerate `zhao_prod_top.sv`** with `tools/quartus/gen_prod_top.py`, then
   `tools/quartus/check_prod_manifest.py`. `CLAUDE.md`: *"Regenerate
   zhao_prod_top.sv after ANY port change… a new port that nobody connects is a
   PINMISSING that only the next fit discovers"* — and it was found stale for two
   separate port changes made days apart. `zero_products_o` is a new port.
7. **Re-run the composed suites**, which are the actual acceptance evidence:
   `perspuv_pairpipe_directed`, `island_composed_directed` (oracle),
   `island_v3_prod_composed_directed`, `island_v3_composed_directed`,
   `island_v3_fault_directed`, and `gate3_paired`'s 392 byte-identical records.
   The swap changes a block inside the island, so byte-identical output is the
   claim to check, not an assumption.
8. **Then the island fit** — the only thing that shows the saving in composition,
   and the one step here that is the owner's call rather than texture-gate
   diagnosis.

## What would falsify it at each stage

* **GATE 3 leaf pair**: if the fresh svc row and the candidate row are within
  noise on registers, the 22–32% estimate was wrong and the lever is dead. The
  map pair says −2,400, so this is the check that could still overturn it.
* **The composed suites**: any drift in `gate3_paired` means the candidate is not
  equivalent in composition, whatever the leaf differential said — and the
  differential's one *declared* exclusion (depth-zero U/V) is exactly where to
  look first.
* **The island fit**: registers could fall while ALM or Fmax worsen. Neither map
  row carries either column, so both are genuinely unknown, and the pair-pipe's
  header only *expects* an ALM saving.


---

# CORRECTION: the swap is NOT a module-name change. Three things had to move.

Attempting it on 2026-09-09 refuted my own recipe at step 1. I had written that
the pair-pipe's ports are "a strict SUPERSET of svc (18 shared + zero_products_o)"
and that the swap is therefore "a MODULE-NAME CHANGE". **The superset claim is
true of port NAMES and false of port WIDTHS**, because the script I compared them
with extracted names and discarded widths — a measurement that answered a slightly
different question than the one I asked of it, which is the oldest failure in this
repository's book.

What the toolchain refused, in order:

1. **`occupancy_o` is 5 bits on the pair-pipe and 4 on svc.**
   `WIDTHEXPAND: connection 'occupancy_o' expects 5 bits ... 'pu_occ' generates 4`.
   The extra bit is correct, not a defect: `owned_q` counts everything accepted and
   not yet emitted — the pipeline stages, the terminal FIFO and the item in the
   output — so it can exceed `NTOK = 16`. Fixed by widening `pu_occ` to `[4:0]` in
   both tops. Safe: `pu_occ` is declared, driven and **read nowhere** in either
   island.
2. **`zero_products_o` cannot be left unconnected.** Verilator raises `PINMISSING`
   as an error here, so the one genuinely new port must be wired even to simulate.
   Landed on a local `pu_zero_products` for the simulation check; production should
   carry it to a debug counter port, exactly as this recipe's step 3 already said.
3. **Four test source lists needed the new file** (`tests/CMakeLists.txt` at 975,
   2205, 2259, 2479), or the island tests fail with the module missing while the
   file sits in the tree — the `MODMISSING` trap `CLAUDE.md` documents.

4. **`zhao_prod_top`'s source list needed it too** — a FOURTH wall, and the one
   that would have cost a fit rather than a minute.
   `check_prod_manifest.py`: *"reachable from the generated production top
   (instantiated by a submodule, not by the top itself) but is NOT in
   zhao_prod_top's source list in design/fit_targets.yml — **the fit would die at
   elaboration**"*. That is `CLAUDE.md`'s rule stated by a tool: registering a
   block in the ledger, the manifest and the source list are three different
   acts. The checker only catches it because it walks the TRANSITIVE closure —
   the pair-pipe is instantiated by the island, not by the top.

So the real change is **nine lines across five files**, not two. Still small,
still reversible, still a drop-in in the sense that matters — no logic, no
protocol and no connection semantics changed — but the recipe as published would
have failed on its first attempt, and anyone following it would have hit the same
three walls.

**The port-width table I should have produced in the first place:**

```
shared ports: 18      WIDTH MISMATCHES: 1
   occupancy_o     svc [3:0]     pairpipe [4:0]
only in pairpipe: ['zero_products_o']
```

That is the whole delta, and it takes one script to get right. The lesson is not
"compare widths too" — it is that **a comparison is only as good as the field it
actually read**, and mine silently answered "are the names the same" while I
reported "are the ports the same".
