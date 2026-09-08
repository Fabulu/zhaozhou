# D0 repaired, and packet C's real account

2026-09-08. Follows `reports/D0-JOIN-SEAM-REPRODUCED-20260908.md`.

## The repair

One gate in `zhao_texture_metajoin.sv`:

```systemverilog
if (rd_valid_i && rd_legal_c) begin
  rd_q     <= mem_q[rd_addr_c];
  rd_gen_q <= rd_owner_gen_i;
end
rd_v_q   <= rd_valid_i && rd_legal_c;
```

What makes it the *right* gate rather than merely a working one: `rd_valid_i` is
wired at `zhao_texture_island_v3_top.sv:2716` as `cache_smp_valid && r1_room_c`,
which is the **same expression** that enables the join's `r1_d_q`/`r1_t_q` at
line 1637. Bank and join now load on identical cycles, so the metadata belongs
to the data beside it *by construction* rather than by timing coincidence. That
is the "whole-record holding" the brief asked for, and it falls out of one
condition rather than needing a second holding register.

It also un-blinds the generation check, for the reason given in the previous
report: inside the gate, the captured generation belongs to the same read as the
row.

## Result

`tests/texture/metajoin_seam_directed.cpp`, **7 of 7 pass** (was 4 failing):

```
after stall with B offered: pal_gen 30 (A=30, B=7C), format 2 (A=2, B=5)
gen-mismatch on a genuine stale read: 0 -> 1
after an ILLEGAL read: pal_gen 30 (was 30), valid 0
```

A held, A's metadata. The illegal key no longer disturbs the outputs. The
generation detector still fires on a genuine staleness, so it was not silenced
to make the test pass. `metajoin_directed`'s original 7 checks are unaffected.

The three D0/D0c checks are the **mutant catchers**: remove the gate and they go
red again, which is what "keep the failing mutant" means when the mutant cannot
live in the tree.

## Why 392 byte-identical paired records did not catch this

They cannot. D0 requires the dispatcher to stall the join, and gate 3's paired
workload runs the response path without that backpressure — so the offered
address never differs from the held one and the swap has no opportunity to
happen. A gate that never reaches the state is not evidence about the state.

This is the session's own lesson arriving a third time: *component checks passing
is not likeness evidence*, and *counters see what pictures cannot* — except here
even the counters could not see it, because the one counter positioned to notice
was differencing two operands that moved together.

## Packet C's real account

Both receipts are now anchored — clean trees, matching source commits — so
`tools/quartus/packet_accounting.py` will print all four sections for the first
time. Stage C (`bcdfadea`) → packet C (`1b81c013`):

```
1. WHAT DISAPPEARED FROM THE SOURCE
   26 lines removed
     - counter bump   3 removed

3. WHAT REPLACED IT
   923 lines added
     + always_ff      7 added
     + generate       3 added
     + instantiation  1 added
     + array decl     3 added
     + counter bump   16 added

2/4. WHAT THE MEASUREMENTS SAY
   alms             13133 -> 15483    +2350
   registers        20561 -> 22219    +1658
   ramBlocks           45 -> 48          +3
   blockMemoryBits  54014 -> 64762   +10748
   dspBlocks           17 -> 17          +0
   virtualPins       1491 -> 1839      +348
   Fmax             82.41 -> 62.83   -19.58 MHz
```

That is the honest account, and it is not a good one. Packet C removed
twenty-six lines and added nine hundred and twenty-three; it added seven
`always_ff` blocks and thirteen net counter bumps; it cost **+2,350 ALM** and
**−19.58 MHz**; and it shipped a metadata-swap defect that its own gates could
not reach.

The Decrufter's thesis was that this island needs gutting. Packet C was the
opposite operation, and calling it "PACKET C COMPLETE" measured the wrong thing —
the gates it passed, rather than what it did to the machine.

Two notes on reading the numbers above:

* `@pktC-fixed` (15,483 / 62.83, clean, `1b81c013`) supersedes the `@pktC` row
  (15,911 / 73.98) entirely: that one was fitted from a dirty tree and cannot be
  quoted. The delta between them is not a measurement of anything.
* No Fmax **attribution** is claimed. The owner brief holds that matching
  worst-path families is neither necessary nor sufficient for it, and −19.58 MHz
  across 923 added lines has no single cause on the evidence in hand.
* `@pktC-fixed` is a labelled row, and labelled rows are never rule-checked. Its
  empty `ruleViolations` is silence, not compliance — on the unlabelled row the
  same design breaks three budget rules.

## What this does not settle

The repair makes the record atomic. It does **not** address D0d — the owner
generation still has no output port, and the island still packs `8'd0` where it
would go, so no downstream stage can independently observe a stale join. That
belongs with §14.4's typed early descriptor, where the field has somewhere to
travel.
