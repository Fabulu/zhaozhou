# RCP24 V3 — the exact-multiplier tile, fitted

Measured 2026-09-06, `zhao_raster_rcp24_v3`, source digest `6553c5cef196` —
byte-identical to the `@v3-after2` map-only row and to the bytes every test
measured, so synthesis, tests and fit refer to the same code (V3 §26.2's
acceptance condition, and the reason the digest field exists).

## The numbers

| | `_svc` before | **`_v3` full fit** | |
|---|---|---|---|
| ALM | 1,041 · 1,037 · 1,038 | **1,230** | +18% |
| fmax | 68.46 · 68.63 · **63.93** | **90.54 MHz** | **+35%** |
| registers | 1,101 | 1,944 | +77% |
| DSP | 6 | **3** | halved |
| M10K | 0 | **6** | |
| virtual pins | 177 | 276 | |

**The old block was fitted THREE TIMES on unchanged source** — 63.93 to 68.63
MHz, a 4.7 MHz spread. That was done deliberately before any comparison, and it
is the only reason the +23.5 MHz below can be called architectural rather than
placement luck. One number against one number would have proved nothing.

## The fmax is INTERNAL, which is the strong case

Splitting all 1,734 summarised paths by origin, because this repository has been
caught both ways by not doing it:

| origin | count | worst slack | implied |
|---|---|---|---|
| starts at a PIN | 1,695 | −0.105 ns | 98.96 MHz |
| **starts INSIDE the design** | 39 | **−1.045 ns** | **90.54 MHz** |

The boundary is at 98.96 and the design's own logic is at 90.54, so **the
reported fmax is set by real logic and deleting the fit boundary entirely would
buy nothing.** That is the opposite of the composed island, where 3,715 of 3,904
paths started at a pin and the boundary was worth ~9.7 MHz. The split is cheap
and its answer is not stable between blocks, which is exactly why it gets run
every time rather than inherited.

Note the pin count ROSE 177 → 276. More pins normally means more pin-limited
paths and a worse number; this block got faster anyway.

**The next limit is named:** `zhao_raster_ticketq:u_doneq|mem_q[6][3] →
r_tok_o[2]` — the done-queue's memory read to its output token. Not the
multiplier, not the scans that were removed.

## What was traded

+189 ALM and +843 registers for −3 DSP and +23.5 MHz. Six M10Ks appeared where
there were none, so part of that register growth is state that moved INTO
memory rather than state that was added.

Against V3 §21's gates this tile is not the constrained one — the redlines
(7,500 ALM / 9,000 registers / 14 DSP) are whole-island figures. The DSP number
is the one that matters here: the composed island breached its rule at 17
against 14, and halving one block's six is a real contribution to closing it.

**§10.7's stated target was TWO DSP blocks. Three were measured.** That is a
miss, recorded as a miss, and the fit-target rule is set at the measurement
rather than at the target so the gate reflects what exists.

## What this does not establish

Not composed with anything — this is the tile alone, and V3 §26.1 is explicit
that it must not be merged into the composition until the shared record and
credit contracts are fixed. The negative-correction path that the exact
reduction exists for is **unreachable from real denominators** (max `w` =
0x401F_EF88 against 2^31; a paired denominator test passes with the correction
deleted), so it is exercised through the arithmetic core's own port, not
through the block's normal input.
