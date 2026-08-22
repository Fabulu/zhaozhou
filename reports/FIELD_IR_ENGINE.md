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
| Arithmetic core | 15 opcodes, `0x00`–`0x0C`, `0x10`–`0x11` | `zhao_field_alu.sv` | 1,005 | 140,000 | **31 / 34**, 3 equivalent |
| Reciprocal | `RCP` | `zhao_field_rcp.sv` + generated ROM | 329 | 60,000 | **23 / 23** |
| Sine / cosine | `SIN`, `COS` | `zhao_field_sin.sv` + generated ROM | 20 | exhaustive | — |
| Length / distance | `LEN2`, `LEN3`, `DIST2` | `zhao_field_len.sv`, `zhao_field_isqrt.sv` | 159 | 9,000 | — |
| Normalise | `NORMALIZE2`, `NORMALIZE3` | `zhao_field_normalize.sv` + generated ROM | 419 | 13,522 | **7 / 7** |
| Table ops | `CURVE`, `DCURVE`, `SPLINE` | `zhao_field_curve.sv` | 11,863 | 21,000 | **18 / 18** |
| Lattice noise | `NOISE2`, `RIDGE` | `zhao_field_noise.sv` | 346 | 12,000 | **15 / 17**, 2 equivalent |
| Rotation | `ROT2`, `ROT3` | `zhao_field_rot.sv` | 3,495 | 15,000 | **17 / 17** |
| Band | `RING` | `zhao_field_ring.sv` | 572 | 24,000 | **17 / 18**, 1 equivalent |

Tests are `tests/differential/field_<piece>_directed.cpp`. The "directed" column
is the check count with no arguments; "random" is the count added by the fast
lane's `--random` argument, and each nightly lane runs the same test with a
larger draw. The sine lane has no `--random` argument because it sweeps **all
65,536 angles** and reports the sweep as one check.

Mutation scores now exist for seven of the nine pieces. **TWO REMAIN UNSWEPT**
— Sine/cosine and Length/distance — built before the sweep harness existed.
That is a gap, not a claim of coverage, and it is still worth closing.

### The reciprocal, swept 2026-08-22

23 mutations, **23 caught, 0 survivors**, every one by the DIRECTED lane rather
than only by the 20,000-case random draw. They attacked the parts a large
random draw over a nearly-right implementation will happily agree with: the
zero case and its own `rcp0` ledger lane, sign handling on both the magnitude
and the result, the normalisation exponent, the seed-ROM index, the Newton
correction's `2^48` constant and its sign, the `rescale_u(.,47)` round-half-up
constant and shift, the Q16 shift, the exponent rescale's rounding, and the
saturation bound with its lane.

**The equivalent mutant the block already documents was confirmed, not
rediscovered.** `zhao_field_rcp.sv` records that removing the `e == 0` guard
survives the whole suite, and explains why: `e == 0` happens only for
|a| == 1, and 1/(1/65536) is 2^32, which saturates whatever the shift did. That
note was written before this sweep existed and it held up — which is the point
of recording equivalents rather than leaving them to look like holes.

### The arithmetic core, swept 2026-08-22

34 mutations, **31 caught, 3 equivalent, 0 real gaps**, and every one of the 31
was caught by the DIRECTED lane rather than only by the 20,000-program random
draw. The mutations attacked the laws rather than the arithmetic: where
saturation triggers, which direction rounding goes, which ledger lane records
it, CLAMP's operand order, each CMP predicate's boundary, MAD's `c <<< 16`,
DOT2/DOT3's per-lane terms.

**Two directed-lane holes were found and closed.** Every DOT2 case used a `b`
vector whose first two elements were equal — `{1.0,1.0,1.0}`, `{MAX,MAX,MAX}`,
`{0.5,0.5,0.5}` — so a block computing `a0*b0 + a1*b0` instead of
`a0*b0 + a1*b1` answered every one of them correctly. That is visible by
inspection and needed no sweep to justify. Asymmetric cases added for both
DOT2's second term and DOT3's third.

**The three survivors are equivalent by algebra**, and the argument is in
`zhao_field_alu.sv` beside the bound: clamping a value that already sits
exactly on the rail returns that same value, so `v > MAX`, `v > MAX-1` and
`v >= MAX` cannot be distinguished by any input. What makes that safe rather
than merely untested is that the LEDGER bound lives in a separate function,
`sat32_fired`, whose own mutations were caught both ways.

**A warning about this sweep's method, because it nearly produced a confident
wrong answer.** Three earlier runs of it reported different scores — 31/34 with
four "caught only by the random lane", then a spurious equivalence, then a
false staleness abort. All three were artefacts of ONE defect: `ninja` could
not regenerate `build.ninja` (cmake reads `VERILATOR_ROOT` from the
environment and it was unset), and when that happens **ninja builds nothing at
all** while still printing plausible output. Every mutation then tested
whatever binary was lying around. The harness now treats
`rebuilding 'build.ninja'` as fatal. Only the fourth run, on a sound build, is
recorded here.

## What is not built

**Every op is built.** What remains is the sequencer itself: the register file
and the instruction walk that turns a `.zprog` image into a run, and the five
`FIELD.SEQ.*` blocks that are that sequencer wearing different profiles.
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

### A sweep that reverts silently is a sweep that lies

The `RING` sweep first reported 17 of 18 caught. It was wrong.

`rcp0_not_sticky` replaced its line with `rcp0_o <= 1'b0;` — text that also
appears in the reset and accept branches — so the uniqueness check refused to
**revert** it. The mutation stayed applied, the next mutation was measured
against a still-mutated design and scored as CAUGHT, and the pristine re-check
came back red at the end.

The binary-hash assertion cannot see this: the binary *does* change every time.
So the sweep now makes **two** assertions per mutation — the hash moved, and the
revert both succeeded **and** left the file byte-identical to a pristine copy
taken at the start. A failed revert aborts the run rather than continuing to
report numbers nobody should trust.

Re-run with the check in place, one of the "caught" results turned out to be a
false positive.

### `RING`'s midpoint lane is dead, and the line stays

Moving the midpoint's saturation from the `rescale` lane to `add` survives,
because **the midpoint cannot saturate**. The exact sum of two `s32` values lies
in `[-2^32, 2^32 - 2]`, and halving with round-half-up lands in
`[INT32_MIN, INT32_MAX]` for every input — verified over 300,000 cases, with
both rails hit exactly and nothing outside.

`sat_rescale_o` is therefore always low for `RING`. The line stays because the
reference records the lane there; the test asserts the lane is low rather than
leaving it unexamined.

The *other* survivor was not equivalent at all: pooling the `rcp` lane into
`mul` survived only because every ring in the test had a span of whole units.
`field_rcp` saturates when the reciprocal exceeds `INT32_MAX`, which needs a
span of a few **raw** units. With those cases added it is caught by 28 checks.

### The rotation ops round TWICE, and that is the law

`fx_sub(fx_mul(c,p), fx_mul(s,q))` rounds each product separately and then
saturates the difference. Everywhere else in this design — `mat4_vec4`,
`fx_mad`, GEOM.SKIN — a row of products is summed exactly and rescaled **once**,
because double rounding is normally the bug. Here the reference does the
opposite.

**The rule is not "single rounding is always right"; it is "match the
reference".** An implementation improved into the house style is wrong, and
about a quarter of random inputs can tell the two apart. The test counts the
inputs where a fused form *would* differ and asserts that count is large — a
sweep on which the two happen to agree proves nothing about which is
implemented.

### Two mutations that survived because they were wrong, not because the test was

Worth recording, because a surviving mutation is only evidence if the mutation
is real:

- **`fused_single_rounding` (first attempt)** was algebraically a no-op.
  `rescale(t·2^16 + p, 16)` equals `t + rescale(p, 16)` exactly, so it
  "survived" while changing nothing. Rewritten to hold the exact 64-bit
  products and rescale once — a coordinated multi-edit, since it needs a wider
  register and every use of it updated — it is caught by 249 directed checks.
- **`sat_lanes_pooled`** survived because the test only compared the
  **collapsed** `Status.sat` bit, which cannot tell an `add` saturation from a
  `mul` one. The test now restates the per-lane attribution and drives the two
  lanes apart; the mutation is caught.

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
