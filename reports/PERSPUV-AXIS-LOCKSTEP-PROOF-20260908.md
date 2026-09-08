# PERSPUV's two schedulers are provably identical — §14.6's precondition

2026-09-08. The Decrufter brief calls PERSPUV *"the largest new opportunity"* and
proposes replacing its two schedulers with **one paired transaction → two
parallel lanes → one paired result**. It also says to *prove the old axis-control
lockstep first*, which is the right order: if the two ever diverge, the whole
rebuild is wrong.

They cannot diverge. This is a closed inductive proof from source, not a sampled
observation.

## The state, and every writer of it

`zhao_raster_perspuv_svc.sv` holds one work queue per axis:

```systemverilog
logic [TW-1:0] wq   [2][NTOK];
logic [TW:0]   wq_wp [2], wq_rp [2];
```

There are **exactly four** assignments to those pointers in the whole file, and
that exhaustiveness is what makes this a proof rather than an argument:

| line | assignment | guard |
|---|---|---|
| 422 | `wq_wp[ax] <= '0` | reset, `for (ax)` |
| 423 | `wq_rp[ax] <= '0` | reset, `for (ax)` |
| 478 | `wq_wp[ax] <= wq_wp[ax] + 1` | `v_valid_i && v_ready_o` then `!depth_zero_i`, `for (ax)` |
| 534 | `wq_rp[ax] <= wq_rp[ax] + 1` | `pk_v[ax]`, `for (ax)` |

## The induction

Let `P(n)` be `wq_wp[0] == wq_wp[1] && wq_rp[0] == wq_rp[1]` at cycle `n`.

**Base.** Reset drives all four to `'0` inside one `for (ax)` loop. `P(0)`.

**Step.** Assume `P(n)`.

* The push guard — `v_valid_i && v_ready_o && !depth_zero_i` — contains **no
  reference to `ax`**. Both pointers therefore increment, or neither does.
* The pop guard is `pk_v[ax]`, which *is* axis-indexed. But
  `pk_v[ax] = (wq_wp[ax] != wq_rp[ax])`, and by `P(n)` both sides of that
  comparison are equal across axes, so `pk_v[0] == pk_v[1]`. Both pop, or
  neither does.

So `P(n+1)`. The two schedulers are bit-identical for all time. ∎

**And so is the queue storage.** Both axes write the same value `tail_q` at the
same index `wq_wp[ax]`, so `wq[0]` and `wq[1]` are elementwise equal over every
written entry — hence `pk_i[0] == pk_i[1]` as well as `pk_v[0] == pk_v[1]`. The
selected token, not merely the go/no-go, is common.

## What this licenses, and what it does not

Licensed: **one** scheduler. One pointer pair, one emptiness test, one token
select, one 16-entry queue. `pk_v`/`pk_i` become scalars and the brief's "one
paired transaction" is exactly the right shape for the control.

**Not** licensed: merging the lanes. The data genuinely differs per axis —
`e_num_u` vs `e_num_v`, `e_mant_u` vs `e_mant_v`, and independent saturation
(`set_u = p3_v_q[0] && sat_c[0]` against `set_v = p3_v_q[1] && sat_c[1]`). The
lanes must stay two. That is what the brief says, and the proof supports that
shape and no wider one.

The saving is mostly **control depth, not bits**: 64 redundant queue bits at
NTOK=16/TW=4 is nothing, but the duplicated pointer arithmetic and emptiness
comparison sit on the path the 2026-09-03 rebuild named as the new limiter
(`pk_i~5_OTERM777 -> p1_prod_q[0][36]_OTERM139`). Any Fmax claim still has to be
measured; this report establishes only that the deletion is **sound**, and
soundness is the thing that was worth proving before touching it.

## One caution recorded against myself

This is a proof that says a deletion is safe — the comfortable direction. Per
this repo's own law, *the first explanation that absolves the design is the one
to check hardest*. The check that makes it hold up is the **exhaustive** writer
table above: the induction is only worth anything because there are no other
assignments to those four pointers, and that was verified by grepping every
occurrence rather than by reading the block's structure and trusting it.

The empirical backstop still belongs in the suite — the invariant asserted every
cycle across a workload including the depth-zero fragments that make `tail_q` and
`wq_wp` diverge from *each other* (the case a comment at line 470 says once cost
one fragment of 335). Blocked for now: `zhao_raster_perspuv_svc.sv` is inside the
running `@pktC-fixed` fit's 19-file closure, and reaching its internals needs a
`--public-flat-rd` verilate flag on a new target rather than any source edit, so
the test lands with the §14.6 work.
