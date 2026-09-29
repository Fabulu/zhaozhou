# Why the uop store does not infer: the READ ADDRESS is combinational

Coordinator, 2026-09-29. **Status: hypothesis with strong in-module evidence and
two falsifiable predictions, one of which is running.** Not yet a settled cause.

---

## The evidence, and it came from inside the same module

The decisive comparison is not between production and a probe. It is between
`store` and the arrays **in the same `always_ff`** that *do* infer. Quartus's own
summary for a standalone map of `zhao_field_v3_exec`:

```
Info (19000):  Inferred 6 megafunctions from design logic
Info (276014): Found 4 instances of uninferred RAM logic
    Info (276007): RAM logic "store"    is uninferred due to asynchronous read logic
    Info (276004): RAM logic "lq_ctx_r" is uninferred due to inappropriate RAM size
    Info (276004): RAM logic "lq_op_r"  is uninferred due to inappropriate RAM size
    Info (276004): RAM logic "lq_dst_r" is uninferred due to inappropriate RAM size
```

So **six arrays in this module become RAMs and four do not**, and `store` is the
only one blamed on the read rather than on size. Now place the two cases side by
side:

| array | how it is READ | READ ADDRESS | result |
|---|---|---|---|
| `lq_s0_r`, `lq_imm_r` | module-scope **continuous `assign`** — genuinely combinational: `assign long_s0_o = lq_s0_r[lq_hd_r];` | **`lq_hd_r` — a REGISTER** (`<=` in the always_ff at `:1052`, `:1240`) | **INFERRED** |
| `store` | **synchronously**, inside the always_ff: `s1_uop_r <= store[...]` | **`issue_ctx_c` — COMBINATIONAL** (`=` in an `always_comb` priority select over `ready_c`, `:379-380`) | **REFUSED** |

**The array with the asynchronous READ infers. The array with the asynchronous
ADDRESS is refused — and refused with a message about the read.** That is
backwards from the message's plain English, which is why five earlier hypotheses
missed it.

## The mechanism

An M10K's address port **is registered**. Quartus can absorb an *existing*
address register into that port; it cannot absorb a combinational expression
without inserting a register, which would change the cycle on which data arrives.
Faced with that, it declines and calls the read asynchronous.

**This tree already recorded the mechanism**, in `check_ram_inference.py`'s rule-2
note from the FLOPARRAY work:

> *"Arm p1 keeps the combinational read in full and still infers a Simple Dual
> Port M10K, because the read address was ALREADY REGISTERED a cycle ahead of its
> use and Quartus absorbed that register into the RAM's own read port."*

That sentence describes `lq_s0_r` exactly, and its converse describes `store`.
The knowledge was in the tool's own documentation and nobody read it back — the
failure mode this campaign keeps paying for.

## It also explains why all six probe styles inferred

Every probe style addressed its array from a module **port** (`ctx_i`, `pc_i`).
At the top of a standalone map a port is available at the clock edge, so Quartus
treats the address as registered and infers. **STYLE=5 came closest and still
missed it**: it took the address *through* a flop array (`pc_arr[ctx_i]`), but the
index `ctx_i` was still a port. **Constructing a probe that infers proves nothing
about a module that does not.**

## Two predictions, one running

| variant | change | prediction |
|---|---|---|
| **R1** | the read lifted out of both enables (`!hold_c && !mul_denied_c`, `issue_c`); **address unchanged, still combinational** | **STILL FAILS** — if R1 infers, the enables mattered and this hypothesis is wrong |
| **R2** | address registered one cycle ahead (`issue_ctx_r`, `issue_pc_r`); **enables unchanged** | **INFERS** — if R2 fails, the address hypothesis is dead |

Both are classified with `tools/budget/store_variant_receipt.py`, which asks what
Quartus said about `store` **by name**, because a reduction can make synthesis
delete the array and **deletion is not inference**.

## What this would mean, and what it would not

**Not a saving.** Nothing here says what banking the store recovers. The
`estimatedAlms` difference between parameterisations is a parameterisation
difference.

**Not a free repair, and not an expensive one either — a schedule question.** The
production read is *already* inside a clocked process, so a synchronous RAM does
not inherently need an extra architectural cycle. What it needs is for the issue
**selection** to happen a cycle earlier, so the address is naturally a register.
That is a change to when the scheduler decides, not to how the data flows, and it
is exactly the kind of thing R4 insisted must be priced inside a schedule rather
than counted from a declaration.

**And it generalises usefully to V2 rather than only to this array.** If the rule
is *"an array addressed by a combinational select cannot be banked"*, then a V2
that banks bulk state must make address selection a registered pipeline stage
everywhere — which is a concrete, checkable architectural constraint, and a
cheaper thing to learn now than after a cluster is built.
