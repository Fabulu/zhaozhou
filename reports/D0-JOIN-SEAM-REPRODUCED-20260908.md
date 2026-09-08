# D0 reproduced at the RAM/join seam — and it is four defects, not one

2026-09-08. The Decrufter brief, §14 step 2: *"Reproduce D0's held-A/offered-B
failure at the RAM-join/dispatcher seam ... Do not use the standalone dispatcher
backpressure test as a substitute for this seam test."*

Done. `tests/texture/metajoin_seam_directed.cpp`, 7 checks, **4 failing against
the live tree** — which is the intended state until the repair lands.

## Why the existing test could not have caught it

`rsp_dispatch_meta_directed` stalls the four **class lanes**, which sit
*downstream* of the dispatcher's input. This seam is *upstream* of it:
bank → join → dispatcher. I reported that test's pass as coverage of the join.
It never touched this boundary. The brief was right to name the substitution
explicitly.

## D0 — the swap

`zhao_texture_metajoin.sv` registers its read unconditionally:

```systemverilog
rd_q     <= mem_q[rd_addr_c];      // every cycle, from whatever is offered
rd_gen_q <= rd_owner_gen_i;
rd_v_q   <= rd_valid_i && rd_legal_c;
```

The island's join stage holds `r1_d_q`/`r1_t_q` when `disp_rsp_ready` drops, but
the bank keeps tracking the next offered address. Measured:

```
after stall with B offered: pal_gen 7C (A=30, B=7C), format 5 (A=2, B=5)
```

Response A's data, A's token, **B's metadata**. Every accepted/emitted counter
balances, because the counters never look at the field that moved.

## D0b — the one detector that could catch it is blinded by it

The bank carries what looks like exactly the right guard:

```systemverilog
if (rd_v_q && (rd_q[OGEN_LO +: GENW] != rd_gen_q))
  rd_gen_mismatch_o <= rd_gen_mismatch_o + 32'd1;
```

It never fires. `rd_gen_q` is loaded by the **same ungated assignment** as
`rd_q`, so on the swap both move to B together — and B's stored generation of
course agrees with B's offered generation. The two quantities the detector
differences are corrupted in lockstep, so it is *structurally incapable* of
firing on the defect it appears built for. Measured: `gen-mismatch counter after
the swap: 0`.

This is the cancelling-errors pattern (docket M13) for the third time this
session. It is also why D0 survived a lint-clean block that has a live identity
counter sitting right next to it: the counter is not evidence, and reading it as
evidence is what let me call packet C complete.

## D0c — an invalid key is counted and then used anyway

`rd_legal_c = (rd_sidx_i != 3)`. On an illegal read the counter bumps and
`rd_v_q` goes low — and `rd_q` **still loads from the illegal address**:

```
after an ILLEGAL read: pal_gen 00 (was 7C), valid 0
```

`rd_result_valid_o` reaches the island as `mj_rd_valid`, whose **only** consumer
is the migration shadow comparator at `zhao_texture_island_v3_top.sv:2754`. The
production path wires `.rsp_meta_i(mj_meta_packed_c)` — derived straight from the
data outputs — without ever consulting it. So the production treatment of an
invalid key is: count it, and use its data.

sidx 3 is unreachable from the expander today (fragrob issues sidx 0..2), which
is why this counter had never fired in anger. **A detector that has not been
shown to fire has not been tested**, so the test fires it on purpose; that check
passes, and it now guards a live detector rather than a permanent zero.

## D0d — the owner generation never leaves the bank

The row stores `wr_owner_gen_i` at `OGEN_LO`, but there is no `rd_owner_gen_o`
port, and the island packs a literal `8'd0` into the record's top eight bits
where it would go:

```systemverilog
wire [39:0] mj_meta_packed_c = {8'd0, mj_rd_pal_slot, mj_rd_pal_gen, ...};
```

The identity that would let a *downstream* stage notice a stale join exists as a
counter inside the bank and nowhere in the data path. Combined with D0b, there
is no surviving mechanism anywhere that can observe a metadata/identity
mismatch.

## The repair

Gate the output register on an actual read, so the record holds as a whole:

```systemverilog
if (rd_valid_i && rd_legal_c) begin
  rd_q     <= mem_q[rd_addr_c];
  rd_gen_q <= rd_owner_gen_i;
end
rd_v_q <= rd_valid_i && rd_legal_c;
```

This fixes D0 and D0c together, and it un-blinds D0b: `rd_gen_q` then belongs to
the same read as `rd_q` rather than to whatever is being offered. D0d needs a
port and is the natural first passenger on §14.4's typed early descriptor.

**Blocked on the live-tree trap.** `zhao_texture_metajoin.sv` is one of the 19
files in the running `@pktC-fixed` island fit's closure (QUARTUS_GOTCHAS §11),
so the RTL edit waits for that fit to exit. The test is outside the closure and
is committed now, red, with this report as its explanation.

## What this costs the last receipt

The `@pktC` milestone fit — 15,911 ALM / 73.98 MHz — measured this arrangement.
Per the brief's own standard, it is a measurement of a **defective** circuit and
must not be carried forward as the packet's cost. My earlier "packet C is
genuinely complete" was premature; so is any Fmax attribution resting on it.
