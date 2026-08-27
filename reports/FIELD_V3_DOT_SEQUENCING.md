# DOT2 and DOT3 on one multiplier: the cycle analysis before the RTL

> 2026-08-28. The v3 executor implements every ALU op except DOT2 and DOT3,
> and refuses those explicitly rather than answering them with a zero product.
> This is the design for lifting that, worked out before any RTL, because the
> multiplier's latency decides the shape and getting it wrong is a wrong
> answer rather than a slow one — which is exactly what happened when the ALU
> was first placed one stage early.

## The constraint that forces sequencing

`reports/Fieldv3.md` budgets the vector multiply bank at **"four 33-bit lanes
map to about 12 DSPs"** — three DSPs per 33×33 signed product, so **one
multiplier per lane**. A DOT2 needs two products and a DOT3 needs three.

So they must be **sequenced**. Replicating the multiplier is the alternative
and it is not free: three per lane is twelve for four lanes, roughly 36 DSPs
against the device's 112 and against a v2 complete engine that already spends
15. That trade should be *measured* before it is taken, not assumed away —
but sequencing is the budgeted design and it is what this increment builds.

## What the multiplier actually does, measured from its source

`zhao_field_mul` is **two clocks deep and fully pipelined**:

```
  issue_i at T   ->  a_q, b_q registered at T+1
                 ->  p_o, p_valid_o at T+2
```

It accepts a new issue every clock — `a_q`/`b_q` are overwritten on each
`issue_i` — so back-to-back issues produce back-to-back products two clocks
later. That is the property the whole schedule below rests on, and it is read
off the RTL rather than assumed.

## Where the operands are, clock by clock

This matters because **the register file's outputs are valid for exactly one
clock**. The datapath already captures them:

| clock | stage | `a1`/`b1` live in | `a2`/`b2` live in |
| --- | --- | --- | --- |
| S2 | operands present | `rf_a1` / `rf_b1` | `rf_a2` / `rf_b2` |
| S3 | captured | `s3_a1_r` / `s3_b1_r` | `s3_a2_r` / `s3_b2_r` |
| S4 | carried | `s4_a1_r` / `s4_b1_r` | `s4_a2_r` / `s4_b2_r` |

So the three products can be issued from three different stages without any
new storage:

```
  issue a0*b0  at S2, from rf_a0/rf_b0      -> product lands at S4   (already built)
  issue a1*b1  at S3, from s3_a1_r/s3_b1_r  -> product lands at S4+1
  issue a2*b2  at S4, from s4_a2_r/s4_b2_r  -> product lands at S4+2
```

## The schedule, and what it costs

The instruction sits at S4 when its first product arrives. The later products
arrive one and two clocks after that, so the instruction must be **held at S4**:

| op | products | extra clocks held at S4 |
| --- | --- | --- |
| MUL / MAD | 1 | 0 |
| DOT2 | 2 | 1 |
| DOT3 | 3 | 2 |

**A DOT3 therefore costs three clocks of multiplier occupancy and stalls
issue for two.** With eight contexts barrelling, the stall is absorbed unless
DOT density is high — which is the number the composition test must measure,
not this note.

The accumulator is 66 bits, matching the multiplier's output width, and the
sum is formed at full width before the ALU's rescale. That is not an
optimisation: adding three products at 66 bits and rescaling once is a
different answer from rescaling each product and adding, and `zfield`'s
`dot2`/`dot3` semantics are the single-rounding form.

## The three ways this goes wrong, named in advance

1. **A product attached to the wrong instruction.** This already happened once
   in this block, and only `desync_o` caught it. The existing guard compares
   the multiplier's `p_valid_o` against S4 every clock; with sequencing that
   comparison must become "the expected number of products arrived for THIS
   instruction", not a bare equality, or it will false-positive on every DOT.
2. **Operands read after their stage has moved on.** `rf_a1` is live for one
   clock. Any schedule that reaches for it later gets whatever the next
   instruction is reading. The table above exists to make that impossible to
   get wrong by accident.
3. **The stall leaking into the barrel.** Freezing S4 must freeze issue too,
   or a new uop enters the pipe behind a held instruction and the two collide
   at the write port. The pinned occupancy counts (65 and 126 clocks) will
   move when this lands, and they should be **re-pinned to the new measured
   values** rather than loosened.

## How it will be verified

Unchanged from the rest of this block: the same FPLAN through
`zfield::execute_point` and through the RTL, with programs that contain DOT2
and DOT3. The existing test already asserts `unsupported_o` goes high for a
DOT — **that assertion inverts** when this lands, which is the signal that the
scope note in the RTL header and the test have been updated together rather
than one drifting from the other.

Mutants this will need, at minimum: each product dropped from the sum; the
sum rescaled per-product instead of once; the hold released one clock early
for DOT3; issue not frozen during the hold; `a2*b2` issued for a DOT2.

## What is NOT decided here

Whether sequencing is the right trade at all. It is the budgeted one, and the
budget came from a DSP count rather than from a measured DOT density in real
Earth programs. `crater_ring` reports 18 `vmul_slots` against 13 `vec_issue`,
which is the closest thing to evidence and says multi-product ops are common
in at least one shipped program. If the composition test shows the stall
dominating, the honest response is to measure a three-multiplier lane and
compare, not to tune this schedule.
