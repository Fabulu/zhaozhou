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
| Sine / cosine | `SIN`, `COS` | `zhao_field_sin.sv` + generated ROM | 20 | exhaustive | **20 / 20** |
| Length / distance | `LEN2`, `LEN3`, `DIST2` | `zhao_field_len.sv`, `zhao_field_isqrt.sv` | 159 | 9,000 | **21 / 21** |
| Normalise | `NORMALIZE2`, `NORMALIZE3` | `zhao_field_normalize.sv` + generated ROM | 419 | 13,522 | **7 / 7** |
| Table ops | `CURVE`, `DCURVE`, `SPLINE` | `zhao_field_curve.sv` | 11,863 | 21,000 | **18 / 18** |
| Lattice noise | `NOISE2`, `RIDGE` | `zhao_field_noise.sv` | 346 | 12,000 | **15 / 17**, 2 equivalent |
| Rotation | `ROT2`, `ROT3` | `zhao_field_rot.sv` | 3,495 | 15,000 | **17 / 17** |
| Band | `RING` | `zhao_field_ring.sv` | 572 | 24,000 | **17 / 18**, 1 equivalent |
| Shared engine | all 31 opcodes | `zhao_field_exec_shared.sv`, `zhao_field_mul.sv` | 1,127 | 12,906 | see below |

## 2026-08-23: ten calculators became one engine

Every row above was built as an independent block with its own multiplier, and
that was the right way to build them -- each one was verified against the
interpreter before it was wired to anything. It was the wrong way to SHIP them.

`zhao_field_seq` retires **one instruction at a time**, so nine of the ten units
were idle at every instant while holding silicon. The first synthesis ever run
on this subsystem measured **79 DSP blocks of a 112-block device** -- 71% of the
chip for one subsystem.

They now share `zhao_field_exec_shared`: one signed 33x33 multiplier lane, one
integer square root, one sine table, one reciprocal, and the two DIFFERENT
reciprocal seed ROMs (`FIELD_RCP_T0` seeds a 32-bit reciprocal with one
correction step, `RCP24_T0` a 24-bit one with two; feeding either function the
other's table would be invisible until some normalised vector came out short).

**79 DSP blocks -> 3.** The production Field cone contains exactly one
nonconstant `*`.

**And simple ops still cost six clocks.** The three register-read cycles were
idle; they are now the lane's issue slots, and the first operand group is read
in `Q_LATCH` from the instruction memory's own outputs rather than a cycle later
from the latched fields -- which, with a two-cycle lane, puts DOT3's third
product in `Q_EXEC`, the state that consumes it. MUL, MAD, DOT2 and DOT3 retire
in six clocks on a machine with one multiplier; the ops that lengthened are the
per-sample ones, worst case NORMALIZE3 at 67 clocks.

Each block's differential still drives that block, through a harness wrapper in
`tests/rtl/` that supplies the shared resources and no semantics. What is new is
what a shared engine newly admits and a parallel one could not: an operation
leaving state behind for the next one. `field_seq_directed` section 13 runs every
operation ALONE and then interleaved in both directions and requires every answer
and each of the five saturation ledger lanes to match; section 14 pins WHEN a
long operation commits, because with sequenced units that is part of the contract
too. Both are proven non-vacuous by mutants written to break exactly them.

Tests are `tests/differential/field_<piece>_directed.cpp`. The "directed" column
is the check count with no arguments; "random" is the count added by the fast
lane's `--random` argument, and each nightly lane runs the same test with a
larger draw. The sine lane has no `--random` argument because it sweeps **all
65,536 angles** and reports the sweep as one check.

**EVERY PIECE IS NOW SWEPT.** The gap this section used to describe -- "built
before the sweep harness was written and have not been swept" -- is closed.

| piece | mutations | caught | survivors |
| --- | ---: | ---: | ---: |
| Arithmetic core | 34 | 31 | 3 equivalent |
| Reciprocal | 23 | **23** | 0 |
| Sine / cosine | 20 | **20** | 0 |
| Length / distance | 21 | **21** | 0 |
| Normalise | 7 | 7 | 0 |
| Table ops | 18 | 18 | 0 |
| Lattice noise | 17 | 15 | 2 equivalent |
| Rotation | 17 | 17 | 0 |
| Band | 18 | 17 | 1 equivalent |

### Sine / cosine, 2026-08-22 -- 20 of 20

COS is SIN a quarter turn on, the quadrant split, the mirror on odd quadrants,
the sign on the upper half, the table index and its fraction, the endpoint
clamp, and every part of the interpolation: slope direction, the round-half-up
constant, the shift, and whether the interpolated term is added at all. The
directed lane sweeps ALL 65,536 angles, so there is no random lane to hide in.

### Length / distance, 2026-08-22 -- 21 of 21

DIST2's subtract-before-square and its own `add` ledger lane, LEN2 vs LEN3's
third component, the saturating difference at both rails, the absolute value
before squaring, the sum of squares being unsigned and exact, the root's
saturation bound, and both output ledger lanes including the flag that has to
RIDE the 34-cycle root rather than be sampled at the end.

### A CAVEAT ON THESE SWEEPS' LANE ATTRIBUTION, withdrawn rather than repeated

The harness prints whether a mutation was caught by the directed lane or only
by the random one. **That attribution is not reliable and is withdrawn.**

On the length sweep it reported `len3_third_dropped` as caught only by the
random lane. Applied by hand, the DIRECTED lane fails two checks on it:

    FAIL: |(2,3,6)| is exactly 7.0: value: expected 0x70000, got 0x39B05
    FAIL: |(3,4,12)| is exactly 13.0: value: expected 0xD0000, got 0x50000

That is the second time the per-lane tag has been wrong while the
caught/survived verdict held -- the first was on the arithmetic core, where the
cause turned out to be a build that was not rebuilding. That cause is fixed and
this one is not explained.

**The caught/survived counts above are what these sweeps establish.** Which lane
did the catching is not, and it is better to say so than to repeat a number I
have twice found to be wrong.

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

### Wave 4 measured, and the path re-ranked the waves AGAIN, 2026-08-25

| | wave 3 (`01598b3`) | wave 4 (`7396df3`) |
| --- | ---: | ---: |
| Fmax | 33.98 MHz | **36.84 MHz** (+8.4%) |
| ALMs | 4,821 | **4,673** (-3%) |
| registers | 3,459 | 3,498 (+39, the added stage) |
| DSP / M10K | 3 / 4 | 3 / 4, unchanged |

Provenance clean: `sourceCommit` equals HEAD, `rtlCleanAtHead` true, 45 sources
hashed, 977.3 s.

**A real gain and a modest one.** The prediction recorded before the run was
that removing the opcode from the write-port address path would help but not
reach 100 MHz, because the long arithmetic units were untouched. That held.

**But the endpoint moved and the CAUSE did not.** The new worst path is

    i_op[7]~DUPLICATE -> walk_wdata_q[27]   26.946 ns  (was 29.250 ns)

which is the same opcode-driven result selection, now landing in wave 4's own
pipeline register instead of the memory write port. Wave 4 bought the 2.3 ns of
routing and write-port setup it was aimed at, and nothing more, which is exactly
what registering an endpoint can buy.

**Attributed rather than guessed: 80 of the path's cells are inside `u_sin`,
and ZERO are in any other unit** -- not `u_isqrt`, `u_rcp`, `u_curve`,
`u_noise`, `u_ring`, `u_rot`, `u_alu`, `u_mul`, `u_norm` or `u_len`. It is a
ripple-carry chain, `cin`/`cout` repeating the length of the cone.

So **wave 5 (SIN) is next, not wave 2 (isqrt)**. The ruling's wave order puts
isqrt first, and isqrt contributes nothing measurable to the current worst path.
This is the second time the measured path has re-ranked the plan -- the first
was wave 4 itself being promoted ahead of wave 2 at 33.98 MHz. Reading the order
instead of the report would have spent a day on the wrong unit, twice.

### The sequencer's registered write-back, wave 4, 2026-08-25

The walk's register-file write is delayed by one edge to get the opcode out of
the write-port address path. The valid bitmap moved with it, driven from the
same registered enable. A comment claimed that setting the bit at the OLD edge
"would open a window where valid is 1 while the memory still holds the previous
value" — presented as a hazard the new placement avoided.

**That claim was wrong, and the ledger's V20 rule is what forced it to be
checked.** The rule refused an invariant with no named enforcer; rather than
name a plausible one, the variant was built and run.

| variant | binary hash changed | result |
| --- | --- | --- |
| valid set at the pre-delay edge | yes, `4ffcaa51` -> distinct | **passes all 1,127 checks** |
| valid assignment deleted | yes, `a3dd0343` | **fails from check 1.one add** |

The first is a **proven-equivalent mutant**: the decoder leaves two edges
between a write-back and the next operand read, so nothing is ever inside the
one-cycle window where bit and datum would disagree. The second establishes the
bit is load-bearing, so the equivalence is not vacuous — without that second
run, "the mutant passed" would have been indistinguishable from "the tests do
not exercise this at all", which is the exact confusion equivalence records
exist to prevent.

The ordering is still worth keeping: it is correct by construction instead of
correct by a slack budget that a later wave could spend. But it is recorded as
a preference with a reason, not as a bug that was fixed.

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

**Every op is built, and so is the sequencer.** `FIELD.SEQ.CORE` is
`RTL_VERIFIED`: all 31 opcodes dispatch under a coverage gate, and the anti-hang
law is formally proven with every instruction word free
(`tests/formal/field_seq_bound.sby`).

**The five `FIELD.SEQ.*` profiles are no longer waiting on anything to be**
**built, because they are not blocks.** Owner ruling, 2026-08-22: *one engine,*
*five profiles*. They are now `kind: profile` in the ledger with
`implemented_by: FIELD.SEQ.CORE`, and rule V21 holds that exemption to its
price — a profile must name its engine, may not out-claim its maturity, and may
not book an ALM budget.

This section used to say the five would "stay SPECIFIED until the sequencer
exists". That framing was wrong in a way worth recording: nothing in the RTL
ever distinguished them. `zhao_field_seq` has no profile input and no
profile-specific port, and what would distinguish a profile — which registers
the input and output lanes bind to — is carried by the DECODED PROGRAM, not by
the block. Five ledger entries at `kind: rtl` were demanding five reference
models and ten test files under V4, and booking **five engines worth of ALM
budget for one engine** under V5.

**What genuinely remains for the profiles is not hardware.** Each needs its lane
binding written down — which registers the inputs arrive in and which the
outputs are read from, per program. That is a software and shell question and it
belongs with the blocks that consume the output. For FLOW it is particle
behaviour, which is reserved to the owner in any case.
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
