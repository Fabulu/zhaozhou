# PERSPUV: where its 3,293 registers are, and why they are not memory

**Date** 2026-09-05
**Block** `zhao_raster_perspuv_svc` (fpga/rtl/raster/zhao_raster_perspuv_svc.sv)
**Rule** islandrearchitecture5.md §8.8 — `<= 900 ALMs`, 700 registers
**Standing measurement** 2,204 ALM / 3,293 registers, and marked **STALE** in
`reports/G1-ISLAND-SURVIVORS-20260905.md`

## THE FIT ANSWERED, AND IT REFUTES THE MECHANISM (2026-09-06)

*Added at the top because the rest of this document is a prediction, and the
prediction was wrong in its central claim. Read this first.*

This report said, in the section below, that the next perspuv fit would confirm
or refute it. **That fit ran, and the answer has been sitting in
`reports/synthesis/zhao_block_fit.json` unread.** The split landed at
`b78996bc` (2026-09-05 12:55) and the fit ran at `5651d96c` (15:15) -- the
split's symbols `e_num_u` / `e_num_v` appear in the fitted source, so this row
measures the change:

| | before the split | after | delta |
|---|---|---|---|
| ALM | 2,204 | **1,910** | −294 (−13.3%) |
| registers | 3,293 | **3,157** | −136 (−4.1%) |
| block memory bits | — | **256** | |
| M10K blocks | — | **1** | |
| Fmax | — | **96.62 MHz** | |

**The split bought a real 13% of ALM and did NOT do what this report said it
would.** The prediction was that `e_num` and `e_q` -- 2,048 bits, "62% of the
block" -- would become memory once each array had one read and one write
address. 256 memory bits inferred in total, across the whole block. 136
registers left. Two thousand bits did not move.

### And the explanation this report talked the next reader OUT of is the
### surviving one

The section below considers QUARTUS_GOTCHAS §14 -- combinational logic between
an array read and the first register blocks absorption into the M10K's output
register -- observes that it is true of `e_q`, whose read is asynchronous and
feeds a port directly:

```systemverilog
  assign u_o = e_q[head_q][0];
  assign v_o = e_q[head_q][1];
```

and then sets it aside: *"it is not the whole story, and stopping there would
send the next person to add an output register that does not fix it. **The
binding constraint is port count.**"*

Port count has now been fixed and nothing inferred. So port count was not the
binding constraint, or not the only one, and **the ordinary explanation this
report steered away from is the one still standing** -- the one
QUARTUS_GOTCHAS already names.

That is the failure mode CLAUDE.md records under *the first explanation that
absolves the design*, with the polarity flipped: here the comfortable
explanation was the more INTERESTING one. A subtle two-port argument displaced a
boring documented gotcha, and it displaced it explicitly, in writing, with a
warning not to go back. The cheap check that separates them is the one that has
now been done by accident: make the port-count change alone and see whether
anything infers.

### What this means for §8.8

The rule is `<= 900 ALMs, <= 700 registers`. At 1,910 / 3,157 the ALM figure is
2.1x over and the register figure is 4.5x over. **The register gap is not
closable by further splitting** -- every array is already single-ported per
axis. The next thing to test is whether registering the read (an output register
on `e_q`, so the path from array to port is not combinational) lets the context
store infer, and that is a MEASUREMENT to make, not a conclusion to write down
here. This report has already spent its credibility on one static analysis.

Not applied: `zhao_raster_perspuv_svc.sv` is inside the composed-island fit
running now (QUARTUS_GOTCHAS 11, the live-tree trap).

---

## What this is and is not

This is **static analysis of the source**, not a measurement. The recommended
next step had been "read the Analysis & Synthesis RAM Summary rather than
re-running a 90-minute fit" — but that fit's workspace lives under `%TEMP%` and
is deleted when the run ends, so there is no RAM Summary on disk to read. The
numbers below are counted from declarations; the mechanism is read off the
access sites. **The next perspuv fit is what confirms or refutes it**, and that
distinction is the point of writing it down before the fit rather than after.

## Where the registers are

With `NTOK = 16` and `TAGW = 16`:

| declaration | bits | share |
|---|---|---|
| `e_num [NTOK][2]` (signed 32) | 1,024 | |
| `e_q   [NTOK][2]` (signed 32) | 1,024 | |
| `e_mant [NTOK]` (24) | 384 | |
| `e_tag  [NTOK]` (16) | 256 | |
| `e_k    [NTOK]` (6) | 96 | |
| `e_have [NTOK]` (2) | 32 | |
| `e_val  [NTOK]` (1) | 16 | |
| **per-token context store** | **2,832** | **~86%** |
| `wq [2][NTOK]` + pointers | 148 | |
| p1/p2/p3 pipeline registers | ~430 | |
| **total accounted** | **~3,410** | |

Measured: 3,293. The small surplus is what synthesis removed. **`e_num` and
`e_q` alone are 2,048 bits — 62% of the block.** A 700-register target is not
reachable while the context store is flops; it is not close.

## Why it did not infer as memory, and it is NOT the usual reason

The habitual answer here is QUARTUS_GOTCHAS §14 — combinational logic between
the array read and the first register blocks absorption into the M10K's output
register. That IS true of `e_q`, whose read is asynchronous and feeds a port
directly:

```systemverilog
  assign u_o = e_q[head_q][0];
  assign v_o = e_q[head_q][1];
```

But it is not the whole story, and stopping there would send the next person to
add an output register that does not fix it. **The binding constraint is port
count**, and it is created by the array's SECOND DIMENSION:

```systemverilog
  logic signed [31:0] e_num [NTOK][2];
  ...
  p1_prod_q[ax] <= 64'(e_num[pk_i[ax]][ax]) * $signed({40'd0, e_mant[pk_i[ax]]});
  ...
  e_q[p3_i_q[ax]][ax] <= q_c[ax];
```

`ax` runs over both axes in the same cycle, and the two axes are driven by **two
independent work queues** — `wq [2][NTOK]` with its own `wq_rp[ax]` per axis, so
`pk_i[0]` and `pk_i[1]` are unrelated addresses. As written, `e_num` therefore
needs **two independent read addresses in one cycle**, and `e_q` needs two
independent write addresses. An M10K offers two ports total. A 2R2W array cannot
be one.

## The consequence, which is the useful part

The second dimension is not shared state. Axis 0 never reads axis 1's entry.
**`[NTOK][2]` is two independent `[NTOK]` arrays wearing one name**, and split
apart each has exactly one read address and one write address:

| after splitting | read address | write address | ports |
|---|---|---|---|
| `e_num_u [NTOK]` | `pk_i[0]` | `tail_q` | 1R1W |
| `e_num_v [NTOK]` | `pk_i[1]` | `tail_q` | 1R1W |
| `e_q_u [NTOK]` | `head_q` | `p3_i_q[0]` | 1R1W |
| `e_q_v [NTOK]` | `head_q` | `p3_i_q[1]` | 1R1W |

1R1W is exactly what a simple dual-port memory does. Each is 16 x 32 = 512
bits, which is MLAB territory rather than M10K — LUT-based memory, so the cost
moves from flip-flops into ALMs rather than into block RAM. That is the right
trade here: the rule perspuv breaches is the REGISTER rule, and its ALM figure
(2,204 against 900) is breached for reasons this does not address.

`e_q` additionally needs its asynchronous read registered before any of this
matters — the port assignment above has to become a registered output stage, or
the split arrays will stay in flops for the §14 reason even once the port count
allows memory.

## What is deliberately NOT claimed

* **Not that this fixes the ALM rule.** 2,204 against 900 is a separate
  overrun and nothing here touches it.
* **Not a predicted register count.** The honest prediction is directional:
  removing 2,048 bits of flops from a 3,293-register block should land it near
  1,250 plus whatever the MLAB addressing costs. Naming a number would be
  inventing precision the analysis cannot support.
* **Not that 90 minutes was a timeout problem.** The recorded defect — elapsed
  time checked only after Quartus returns — is fixed (G0, `run_block_fit.ps1`
  now runs a watchdog job outside the invocation, verified when it interrupted
  a perspuv re-fit at 5,404.9 s against a 5,400 s budget). A block this size
  taking that long is a block this size, not a broken clock.

## Next

`zhao_raster_perspuv_svc.sv` is inside the running composed-island fit's
closure, so it must not be edited until that fit clears (QUARTUS_GOTCHAS §11).
When it does: split the two per-axis dimensions, register `e_q`'s read, re-fit,
and compare the register count against the 3,293 above and the 700 rule.
