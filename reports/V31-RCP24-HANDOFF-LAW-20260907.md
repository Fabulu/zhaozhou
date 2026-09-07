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

Recorded as the sharpest remaining test for this block, rather than claimed as
covered.
