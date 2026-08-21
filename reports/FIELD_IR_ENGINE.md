# The Field IR engine — build report

> Everything here is **simulation**. No part of this has run on a physical
> board. Every number below is reproducible from a committed test in this repo.

The Field IR is the small instruction set that terrain, deformation, particle
flow, formation and stamp programs are written in. One C++ interpreter is the
law (`reference/src/zfield/zfield_interpret.cpp`); `spec/form/field-ir.md` §1
pins op semantics to exactly two implementations, that interpreter and the
TypeScript one, and forbids re-deriving them from prose anywhere else — the RTL
included.

This report tracks the hardware engine that runs those programs. It is built one
op family at a time: RTL, then a differential test against the interpreter, then
a mutation sweep that tries to break the test.

## Why every result carries a mutation score

A differential test that passes proves nothing on its own — it may simply not
look where the bug is. So each piece is finished by deliberately breaking the
RTL in ways a real implementation plausibly would, and checking the test
notices. A mutation the test does *not* catch is either a hole to close or an
equivalent mutant, and equivalent mutants are recorded explicitly so they do not
read as holes later.

**The sweep verifies its own builds.** Verilator in this tree elaborates at
configure time and will serve a cached model after the source changes — observed
here running an old model against a new source, including once against a
*pristine* source. Any sweep that trusts an incremental build can therefore
report a perfect score for a test that never ran. Every iteration rebuilds the
target from scratch and hashes the executable before and after; a result whose
hash did not move is **discarded, never scored**.

## What is built

| Piece | Ops | RTL | Directed | Random (fast lane) | Mutations |
| --- | --- | --- | ---: | ---: | ---: |
| Arithmetic core | 15 opcodes, `0x00`–`0x0C`, `0x10`–`0x11` | `zhao_field_alu.sv` | 828 | 120,000 | — |
| Reciprocal | `RCP` | `zhao_field_rcp.sv` + generated ROM | 329 | 60,000 | — |
| Sine / cosine | `SIN`, `COS` | `zhao_field_sin.sv` + generated ROM | 20 | exhaustive | — |
| Length / distance | `LEN2`, `LEN3`, `DIST2` | `zhao_field_len.sv`, `zhao_field_isqrt.sv` | 159 | 9,000 | — |
| Normalise | `NORMALIZE2`, `NORMALIZE3` | `zhao_field_normalize.sv` + generated ROM | 419 | 13,522 | **7 / 7** |
| Table ops | `CURVE`, `DCURVE`, `SPLINE` | `zhao_field_curve.sv` | 11,863 | 21,000 | **18 / 18** |
| Lattice noise | `NOISE2`, `RIDGE` | `zhao_field_noise.sv` | 346 | 12,000 | **15 / 17**, 2 equivalent |

Tests are `tests/differential/field_<piece>_directed.cpp`. The "directed" column
is the check count with no arguments; "random" is the count added by the fast
lane's `--random` argument, and each nightly lane runs the same test with a
larger draw. The sine lane has no `--random` argument because it sweeps **all
65,536 angles** and reports the sweep as one check.

Mutation scores exist only for the two most recent pieces. The four earlier ones
were built before the sweep harness was written and have not been swept; that is
a gap, not a claim of coverage, and it is worth closing before the sequencer
lands on top of them.

## What is not built

`RING`, `ROT2`, `ROT3` — then the sequencer itself: the register file and the
instruction walk that turns a `.zprog` image into a run.
Until the sequencer exists, the five `FIELD.SEQ.*` blocks in the ledger stay
SPECIFIED, and so do the blocks downstream of them.

None of the remaining engine work needs a decision from the owner. It is the
only large group of blocks in that position.

## Notes worth keeping

### There are two reciprocal tables and they are not interchangeable

`FIELD_RCP_T0` seeds a 32-bit reciprocal with **one** correction step;
`RCP24_T0` seeds a 24-bit one with **two**. Feeding either function the other's
table would be invisible until some normalised vector came out slightly short.
Both are generated from `zref_tables.hpp` rather than typed, and every entry is
checked against the source table.

`RCP24_T0` **descends** — a larger mantissa has a smaller reciprocal — so the
obvious endpoint sanity check (assert the first entry is the smallest) is
backwards for it.

### The zero case is asymmetric between the two normalise ops

`NORMALIZE2` records the `rcp0` ledger lane on a zero vector; `NORMALIZE3`
returns zeros and records nothing. Making them consistent is the obvious
tidy-up and would disagree with every capture the software has produced. Both
halves are pinned, and the mutation that "fixes" it fails.

### The table ops read per-program data, not hardware constants

`CURVE`, `DCURVE` and `SPLINE` are the first ops that read a **table**, and
tables are carried in the `.zprog` image. So `zhao_field_curve.sv` takes a table
port instead of owning a ROM, and the port is a registered read per the M10K
rules.

Three laws in that block are worth naming because each has a plausible wrong
version:

- **The segment search is six steps for every table size**, not
  `ceil(log2(n))`. The decoder caps a table at 64 entries, which is exactly what
  six steps reach.
- **The search runs on the clamped value**, never the raw one, and both bounds
  come from the table's own ends.
- **`SPLINE`'s closing term is `rescale_s32(v, 1)`** — the one-half of
  Catmull-Rom — not `v << 16`. The shift form amplified the term by 2^16 and is
  a fixed defect (review C1, RUN-20260814-1912 wave-1). It is named in the RTL,
  in the test and here so it does not come back.

### The PCG's last step is dead code here, and it stays

`noise2_hash` ends with `(w >> 22) ^ w`. Both `NOISE2` and `RIDGE` then keep
only bits `[31:16]` — and `w >> 22` has nothing above bit 9. **The xor perturbs
exactly the half the ops discard**, so that line cannot change either op's
answer.

Two mutations of it survive the sweep and both are recorded as equivalent
mutants rather than left looking like holes: dropping the xor-shift, and
changing its shift amount from 22 to 21.

It stays in the RTL. The reference is the law and this block is its
differential, not its optimiser — an implementation that agrees on every
observable output while quietly computing something else is the thing this
method exists to prevent, and the day the op is widened to keep more bits, a
"simplified" version would be silently wrong. The test pins the *reason*
directly (section 9) rather than leaving it in a comment nobody re-derives.

### Three gaps the table-op sweep found, and what closed them

The first sweep of `zhao_field_curve.sv` left one survivor and two mutations
that only the random lane caught. All three were real holes in the directed set:

1. **The `[0,1]` clamp on the spline segment parameter survived removal.** On a
   well-formed spline table it is unreachable — but the decoder validates a
   spline table's kind, count, x-order and spacing, and *never* its `dy`. A
   fully decodable program can carry a `dy` that is not `1/step`, which drives
   the parameter out of range. The clamp is what makes the hardware agree with
   the interpreter on such a program, and every well-formed table hides that.
2. **The high end of the clamp was invisible.** The test's table builder gives
   the last knot a slope of zero, because there is no segment after it — so the
   extrapolation term vanishes whether or not the value was clamped, and a block
   clamping only the low end passed every case. Closed with a table whose final
   slope is set by hand.
3. **The single-rounding law was untested at the rails.**
   `rescale(d·dy + (y << 16), 16)` and `rescale(d·dy, 16) + y` are *exactly*
   equal for every value that fits — they part company only where the
   intermediate saturates and the sum would have pulled it back. A directed set
   that never reaches the rail cannot tell the two apart at all.

After those three sections were added, the directed test alone catches all
eighteen mutations.
