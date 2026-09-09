# The island's timing is ONE register bit, and it is not the reciprocal tile

2026-09-09. Written immediately after `GATE4-RCP-LIKE-FOR-LIKE`, because that
memo's leaf numbers invited a conclusion this census kills.

## The conclusion I was about to draw

Gate 4 measured `svc` at 56.24 MHz and `v3` at 100.95 MHz on matched parameters,
with every `svc` row ever fitted between 56.24 and 68.63 and every `v3` row
between 90.41 and 100.95. The island contains `svc` and fits at 62.83 MHz --
suspiciously close to `svc`'s leaf range. The inference writes itself: *the
reciprocal tile is the island's critical path, so the swap buys ~20 MHz.*

**It is wrong, and the receipt to check it was already on disk.** This is the
"first explanation that absolves the design" law with the sign flipped -- the
comfortable story here was that a change I already wanted to make would also fix
timing, and it explained almost all of the evidence.

## The census

`reports/synthesis/blockpaths/zhao_texture_island_v3_top@pktC-fixed.setup.rpt`,
**200 summary paths** (the count Quartus itself declares in the file header),
split by whether each endpoint is inside the design or on the boundary:

| | count | worst slack | implied Fmax |
|---|---|---|---|
| boundary-touching | 157 | -5.915 | **62.83 MHz** (the reported figure) |
| internal-to-internal | 43 | -2.912 | **77.45 MHz** |

Reproduce with `python tools/quartus/path_census.py <rpt> --exclude-instance u_rcp`.

**The island's REPORTED Fmax is set by a pin path that has nothing to do with the
reciprocal**: `pal_ld_gen_i[1]` -> `u_palette|res_r[3]`. The top six paths are all
that same input pin into the palette resolver. Leaf-fit boundary artefact,
already a known category.

## The actual finding: one node sources everything

Of the 43 internal paths:

* **42 of 43 start at `zhao_texture_v3own:u_own|live_cnt_q[6]~DUPLICATE`.** One
  register bit.
* 42 of 43 *end* inside `zhao_raster_rcp24_svc:u_rcp`, almost all in `Add7~*`.
* The single remaining path is `u_rcp|s1_i_q[1]` -> `u_rcp|Mux0`, at -1.417 --
  1.5 ns of slack behind the leader and entirely internal to the tile.

So the reciprocal is **where the path lands, not what makes it long.** The
structure is `live_cnt_q` -> credit comparison -> `credit_available` ->
`v_valid_i` (the island wires `.v_valid_i(frag_valid_i && credit_available)`) ->
the tile's context-launch arithmetic. A high bit of a counter, arriving late out
of a comparator, fanning into someone else's adder.

## What the swap is therefore worth, in timing

Delete every `u_rcp`-terminating path from the census and ask what is next:

```
worst internal WITH    u_rcp paths : -2.912 -> 77.45 MHz
worst internal WITHOUT u_rcp paths : -2.266 -> 81.53 MHz
   and that path is  u_own|live_cnt_q[6] -> u_own|fence_open_q
```

**A perfect reciprocal tile -- one with zero delay -- would move the island's
internal-only ceiling by 4.08 MHz**, and then stop, because the next path leaves
the *same source register* for a destination inside `u_own` itself. The +22 MHz
the leaf rows imply does not transfer, and neither does any part of the +44.71.

**The swap remains justified. The justification is ALM and DSP, not timing.**
-3 DSP is invariant across every row either module has produced, and the ALM
advantage is large and directional. Gate 4's Fmax column is a true statement about
two leaf fits and a false lead about the island.

## What to do instead, and it is cheaper

`live_cnt_q[6]` is the source of **43 of 43** internal paths. Nothing else in the
island's top 200 is internally critical at all. The target is that node's
combinational fanout, not the block it happens to reach:

* the credit comparison should be maintained **incrementally as a registered
  flag** rather than recomputed from the counter each cycle -- an
  increment/decrement already happens, so the flag can move with it;
* `fence_open_q`'s dependence on the same bit is the second path and wants the
  same treatment.

That is a small, local change inside `zhao_texture_v3own`, it targets every
internal path at once, and it costs no DSP and essentially no area. It has a real
correctness obligation -- a registered credit must not permit over-issue on the
cycle it changes -- so it is a designed change with its own directed test, not a
retiming tweak.

**Not done in this pass:** the `@g2-prod` island fit is running and
`zhao_texture_v3own.sv` is inside its closure. Live-tree trap, `QUARTUS_GOTCHAS`
section 11.

## The correction this also forces

`GATE4-RCP-LIKE-FOR-LIKE` closed by saying the NCTX=8 throughput question "has
not been re-asked since the parameters settled". That was wrong -- it was answered
on 2026-09-08 in `RCP-V3-THROUGHPUT-IS-NCTX-DEPENDENT`: **5.78 clk/recip at
NCTX=8, 17% faster than the 6.96 serial reference, below the tile's own 4.6
acceptance threshold, all arithmetic checks passing.** That memo's economic
verdict, though, was written against the mismatched fit pair and said the 17% "has
to pay for 8 M10Ks and +377 registers". Gate 4 retires that half: at matched
parameters `v3` spends **112 FEWER registers**, not 377 more. The M10K cost is
real and is +7.

## A third number was wrong on the way here, and it inflated the comfortable side

The first version of this census matched summary rows on their leading number
alone. `-detail full_path` makes Quartus follow the 200-row summary table with a
per-path node breakdown, and **those 8,813 detail lines begin with a number and a
semicolon too**. The ad-hoc run therefore reported *"442 paths, 399
boundary-touching"*. Both figures were fiction.

The internal count survived by luck: detail rows carry empty node columns, so they
fail the internal test at both ends and fell out of that population anyway. Every
conclusion above is unchanged.

But note **which** number it corrupted. Over-matching detail rows pushed the
boundary share from 78.5% to 90%, making the leaf-fit pin artefact look more
dominant than it is -- the broken instrument leaned toward the comfortable
reading, exactly as the law says. The fix is to require the summary table's exact
arity of eight fields, and `path_census.py`'s self-test now carries a real detail
row as a NEGATIVE control that it must reject. Relaxing the arity makes that test
fail, which was checked rather than assumed.
