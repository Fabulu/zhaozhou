# §16.3's handoff law, checked — including against my own change to it

*2026-09-07. Verification only, no RTL changed. `zhao_raster_rcp24_v3` is outside
the running island fit's closure.*

---

## The rule

§16.3 states it as a hard requirement, and warns about the exact shortcut:

> **No context is freed until the result is safely transferred** under the
> declared handoff law. If a result row is copied into an owned output buffer,
> context reuse is legal only when that buffer holds the complete needed result
> and tag. **A narrow token alone is not a copy of a mutable result row.**

The warning applies directly here, because the DONE queue **does** carry a narrow
token: `done_din` is `CW` wide — a context id, not a result. The results live in
mutable arrays (`res_q`, `p_k_q`, `p_zero_q`, `p_tok_q`) indexed by that id.

## It holds

```systemverilog
assign r_valid_o = !done_empty;
assign r_o       = res_q[done_dout];      // the row, read at the output
assign retire_c  = r_valid_o && r_ready_i;
assign free_push = retire_c;              // the context returns HERE
assign done_pop  = retire_c;
```

The context is returned to the free queue **only on the transfer itself**, not on
entering the DONE queue and not on the result being merely presented. So the row
is still owned while it is being read, which is what the rule demands.

## And the registered head preserves it — which was not free

Today's `zhao_raster_ticketq_rh` gave that DONE queue a registered head plus a
spare. A latency change is precisely the kind of edit that breaks a
"free-on-transfer" law, so it is worth stating why it does not:

* `retire_c` is still `r_valid_o && r_ready_i`. The free is tied to the
  **transfer**, and the wrapper did not move that.
* `r_valid_o` is now `!done_empty` where empty means *no visible head*, so a
  token sitting in the head or spare has **not** retired and its context has not
  been freed.
* The heads can hold **two** tokens. Their rows must stay valid, and they do: a
  context is re-admitted only by being popped from the free queue, and it only
  re-enters that queue at retire. A token still in a head cannot have its row
  overwritten.
* Over-allocation is impossible because the wrapper's `full_o` counts the body
  **and** the heads (§5.3's law), so the DONE queue can never advertise more
  credit than there are contexts.

Empirically consistent too: `raster_rcp24_v3_directed` passes 52 checks with the
wrapper in place, including result-correctness against the reference, and the
before/after clock counts are 4.04 → 4.05 per reciprocal.

## What this is not

Not a proof. It is a structural argument plus a passing differential, and §16.3's
own test list asks for more than either — *"distinct U/V, shuffled completions,
zero/nonzero interleaving, slot reuse, and ready dropping just after a read
launches."* The last of those is the one that would stress this law hardest, and
the directed suite does exercise ready-dropping (`ready 1-in-3`, `1-in-7`) but not
specifically *just after a read launches*.

> **WRITTEN THE SAME DAY.** Naming the sharpest missing test and then not
> writing it would be the same shrug in a smarter costume.
> `raster_ticketq_rh_directed` now drives supply most cycles and drops demand in
> short unpredictable bursts over 4,000 cycles, so a read in flight meets a
> falling ready **by construction rather than by timing luck**.
>
> The property is the one that matters: **every token pushed comes out exactly
> once, and in order.** A lost token and a duplicated token both fail it; a
> merely slow queue does not — which is what separates this from the throughput
> check beside it.
>
> **Fire-tested against the exact defect §5.4's reservation prevents.** With
> `b_pop_c` launching reads regardless of whether a destination is reserved, the
> new check fails by name and the drain count collapses from 16 to 2. 22 checks
> pass on the real RTL.
>
> What is still not covered from §16.3's list: shuffled completions and
> zero/nonzero interleaving, both of which belong to `rcp24_v3`'s own bench
> rather than the queue's.


---

## §16.3's test list, item by item — and it is now complete

The section names five tests for this seam. Rather than leave "what is still not
covered" as a vague tail, each was checked against the actual benches:

| §16.3 asks for | covered by | evidence |
|---|---|---|
| **ready dropping just after a read launches** | `raster_ticketq_rh_directed` | **added today** — adversarial supply/demand over 4,000 cycles; exactly-once and in-order; fire-tested against an unreserved read launch |
| **shuffled completions** | `raster_rcp24_v3_directed` | results are keyed **by token** (`out.got[top.b_tok_o]`), so a token paired with the wrong result lands under the wrong key and mismatches. That is the property, not merely tolerance of reordering |
| **zero/nonzero interleaving** | `raster_rcp24_v3_directed` | zeros are a **scheduled phase**: `if ((i % 97) == 0) d = 0; // keep meeting it` |
| **slot reuse** | `raster_rcp24_v3_directed` | 4,104 requests through `NCTX = 16` contexts — roughly 256 reuses each |
| **distinct U/V** | `raster_perspuv_svc_directed` | a different block; the U/V pair is perspuv's, not rcp24's |

The earlier note in this report said shuffled completions and zero/nonzero
interleaving were "still not covered". **That was wrong** — they were covered,
and saying otherwise without looking would have sent the next pass to write tests
that already exist. Checked and corrected rather than left as a plausible caveat.

So §16.3's verification obligation for the DONE seam is **met**, and the one item
that genuinely was missing is the one that got written.
