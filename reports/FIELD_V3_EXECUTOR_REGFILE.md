# The v3 executor's register file: two shapes, one measurement

> 2026-08-28, written while opening Phase 4. The question is which register
> file the four-wide vector executor is built around. Two documents in this
> tree answer it differently, and the difference is not cosmetic — it decides
> what a multi-member operand costs.

## The two shapes

**`reports/Fieldv3.md`, the hot register file.** Four LANES — the four points
of a vector group — each with three read replicas (A, B, C), each replica
8 contexts × 32 registers × 32 bits = 8,192 bits:

    4 lanes x 3 readers x 1 M10K = 12 M10Ks

**`reports/PIPELINEINGHINTS`, the owner's directive of 2026-08-25**, which is
what `zhao_probe_banked_rf.sv` actually implements. Four BANKS by register
residue (`bank = register[1:0]`) × three replicas, each 16 contexts ×
16 registers × 32 bits = 8,192 bits. The reasoning is operand-shaped rather
than lane-shaped:

> `a, a+1, a+2` touch three DIFFERENT banks, and so do `b, b+1, b+2`; `c` adds
> at most one more read to one bank. So no bank ever needs more than THREE
> reads for one instruction.

## They cost the same and do different things

Both land on **twelve memories of 8,192 bits each**, so the measured fit —
372 ALM, 12 M10K, 0 DSP, 93.14 MHz, positive hold — is a valid measurement of
either one's STORAGE. That is worth stating plainly, because it means the
Phase 3 probe result is not invalidated by this question.

What differs is what one clock can serve:

| | one clock serves |
| --- | --- |
| four lanes × three readers | one `(a, b, c)` triple, for four points |
| four residue banks × three replicas | seven operands `a, a+1, a+2, b, b+1, b+2, c`, for ONE point |

## Why this is not academic: the IR has multi-member operands

`zfield::VecUop` carries `src[9]` with a flattened member count, and the
shipped Earth programs use it. `UOP_RING_PREP` alone is nine multiplier
slots, and the FPLAN differential records `crater_ring` at 18 `vmul_slots`
against 13 `vec_issue` — more multiplies than instructions, which is only
possible with multi-member sources.

So a three-reader-per-lane file cannot serve a three-member operand set in one
clock; those uops must be SEQUENCED across clocks. The brief is consistent
with that elsewhere — it budgets "prepared RING: 9 multiplier slots / group"
as a per-group cost, not a per-clock one — but the 12-M10K line reads as
though one clock serves an instruction, and a reader budgeting issue slots
from it will be optimistic on exactly the programs that matter.

## The decision, and how to reverse it

**Phase 4 builds the executor around the residue-banked shape.** Three
reasons, in order of weight:

1. It is the shape that has been **measured**. 372 ALM / 12 M10K / 93.14 MHz
   with positive hold is a real number against a real device; the lane-shaped
   file is an estimate.
2. It costs the **same storage** and serves strictly more per clock, so the
   choice is not a trade.
3. It is the **owner's directive**, and the directive explicitly asked to be
   checked by Quartus rather than believed — which has now happened.

**Reversing it is cheap and stays cheap** provided the executor keeps the
register file behind a module boundary with a `(ctx, a, b, c)` request and a
seven-operand response. Nothing above that boundary should know how the banks
are cut. If the lane-shaped file is later preferred, the swap is one module.

**What would change the decision:** a fit of the residue-banked file at the
depth the executor actually needs (the probe measured 16 contexts × 64
registers; the brief argues 8 × 32 suffices after uniform elimination, which
is a quarter of the storage). If the smaller depth lets the lane-shaped file
fit in fewer memories, that is a real argument and the number should decide
it — not this note.

## What is NOT claimed here

That either shape is right for the whole engine. The four points still need
four lanes of arithmetic whatever the register file looks like; this note is
only about where the register storage is cut and what one read cycle
delivers.
