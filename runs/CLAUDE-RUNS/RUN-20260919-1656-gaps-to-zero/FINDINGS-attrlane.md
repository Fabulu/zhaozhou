# FINDINGS — ATTRLANE (the GEOM.CLIP → ATTRPACK lit-colour severance)

**2026-09-21. Branch `gz/attrlane` at `319fe5b1`, merged. Register 22 → 22.**
**213 insertions, zero deletions, all comments.**

Transcribed by the coordinator from the lane's commit message; the harness
refused the lane's own write to this path.

> **This file is cited by production RTL** — `zhao_geom_attrpack.sv` and
> `zhao_raster_tile_pipe_v2.sv` both point here for the cost working. It was
> missing for several hours after those comments landed, which packet
> GOURAUDLOOK caught as *"a dangling citation in production RTL."* The debt was
> the coordinator's, not the lane's.

---

## The verdict: SEVERED DELIVERY, not dead computation

Walked by hand rather than grepped from a layout:

```
zhao_light_stream -> geom_light_*_o
  -> zhao_geom_vattr        latched into c_rgb_q, COUNTED by colours_written_o
  -> rep_data_o[95:64] / [127:96] / [159:128]
  -> GEOM.REPLAY            rp_attr_* = {rp_st_*, 8'd0, rp_invw_*}  = slots 3,4,5
  -> zhao_geom_clip         WINDING-SWAPPED with the corners
  -> zhao_geom_attrpack.tri_attr_*_i     <-- arrives FULL and stops
```

**The lighting is not dead silicon, and the proof is structural: GEOM.CLIP
performs a winding swap on those slots that exists for no other purpose.** A
block does not swap what it does not carry. **Reclaiming the computation would
be removing function, and that was refused.**

## The correction — PROJOUT's "either/or" is false

PROJOUT framed the destination as *either* slots 3–5 *or* the continuation
tail's `vertex_rgb`. **They are the same path**, and the error ran in the
direction that made the work look like two jobs.

In `zhao_raster_tile_pipe_v2`'s attribute `always_comb`, `continuation_w` is
assembled **per fragment**, three lines under `attr_join_q_q[1]` and `[2]`. So
**`vertex_rgb` needs no separate I20 producer** — with lanes 3–5 present that
line becomes a field build off `attr_join_q_q[3..5]`, and **nothing downstream
of the tile pipe changes.**

*(Coordinator: this corrects R224's `vertex_rgb` clause. Its disposition of the
other three tail fields stands.)*

## R89 does not transfer, and must not be inherited

R89 refused a fourth plane for **alpha** because the value *"does not vary
across the primitive."* **True of shadow alpha; false of lit vertex colour** —
varying across the primitive is what Gouraud *is*.

R133 drew the right line for alpha: *"the plumbing is finished, only the faucet
is missing"*, and that faucet was unbuildable. **Here the plumbing is finished
and the water is already in the building.**

> **This is refused on COST, not on absence. The next lane must not inherit
> "R89 settled it."**

## The cost — and the lane caught its own error in the flattering direction

**~1,420 ALM and +24 DSP for three lanes.** Per lane: **488.1 / 463.6 / 467.8
ALM and 8 DSP**, from section 17 of
`zhao_raster_texture_v3_fit_top@g8a.fit.rpt` — truth device,
`rtlCleanAtHead: true`.

Plus **`METAW` 1157 → 1877**, taking the binner's metadata bank from **29
forty-bit slices to 47** — M10K *and* per-triangle bank time.

**It first wrote +9 DSP**, taken from the `ATTR_DSP3=1` variant — **G8A
characterization only; the console runs `ATTR_DSP3=0`.** Nearly **3×
understated**, caught by reading the production row by hand.

**+24 DSP is 21% of the entire 112-DSP budget**, on a console
`BUDGET_HEATMAP` already puts at **185 DSP against 112**. That is probably the
figure that decides this, not the ALM.

**Where the number was NOT found matters too:** no leaf row exists in
`zhao_block_fit.json`, where the lane looked first and **wrongly concluded
"unpriced."**

## The unexpected finding: a third built-but-uninstalled stage

**`zhao_raster_toon` takes `r_i`/`g_i`/`b_i` as `signed [31:0]` — exactly
`attr_join_q_q[]`'s shape — with `zhao_raster_fog` behind it. Neither is
instantiated by any composed top**, and `prod_manifest.yml` says so outright.

**A whole per-pixel colour stage is authored and waiting on the far side of the
severance.** That is deferred intent, not new scope, and it settles *subsystem*
over *wiring job*.

## What was built, and what was declined

**Built:** the stand-in is now **named in the RTL at both ends of the
severance** — `zhao_raster_tile_pipe_v2.sv` and `zhao_geom_attrpack.sv` — plus
the corrected I13 entry. PROJOUT had left that instruction explicitly
undischarged and nobody had done it. **The attrpack waiver previously read as
though the slots were *spare*; it now says they arrive FULL.**

**Declined, each with a reason:**

* **The three lanes** — an owner decision, now priced.
* **A hardware drop counter** — because **the oracle already answers it free**
  (`creature_sim.cpp`'s `gouraud = a.lit && b.lit && c.lit`). Spending silicon
  on an over-ceiling device to measure what the reference model computes is the
  wrong trade.
* **Narrowing `GEOM_CLIP_ATTRS`** — PROJOUT's refusal **re-affirmed**, so the
  "four unread slots" finding still cannot be mistaken for spare silicon.

**No tie-off created, so none declared.** `packet_h_tieoff_audit.py` still reads
**0 SILENT**.

## Gates — scaled per R227

All 13 static gates green; both edited RTL files lint RC 0; the plain smoke form
**PASS, RC 0, `%Fatal` grep = 0 lines** (R207 — the exit code is not the
evidence).

**Not run, with reason given:** the other smoke forms (behaviour unchanged; a
comment can only break elaboration, which one form proves) and all four
generator checks (**no port changed**, so they cannot move).

## Two process notes

**It started a leaf fit and stood down.** `PACKET-PROTOCOL.md` says *"Do not run
Quartus"*, which **conflicted with its brief's "price it in ALM"** — and **the
protocol won**, which is the right precedence. It refused at preflight before
spending Quartus time, and the number was already on disk. *The conflict was the
coordinator's.*

**The owner question it hands up:** does the console ship Gouraud vertex colour,
or a named flat stand-in? *(Answered in part by GOURAUDLOOK, which found there
are **three** readings, not two — and that the free one is neither of the two
this lane weighed.)*
