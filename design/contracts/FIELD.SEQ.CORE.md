# Contract — FIELD.SEQ.CORE (Field IR sequencer core)

> Ledger: `design/blocks.yml` · owner ZH-078 · phase 7 · maturity UNIT_VERIFIED

## Purpose and exclusions

The register file and the instruction walk that turns a decoded Field IR program
into a run: zero the file, load the declared input lanes, walk instructions
until `OP_END`, read the declared output lanes.

This is the shared body of all five `FIELD.SEQ.*` profile blocks. The profile
decides which ops a program may contain; the **decoder** enforces that. There is
no per-profile hardware difference in the walk itself.

**Not here:** the op semantics, which live in their own blocks; the program
cache, which is `FIELD.PROGCACHE`'s; and the lane MAP, which is per-program
metadata. The host writes input registers and reads output registers through the
file's own port, so the map stays where it belongs — with the program.

## Clock and reset semantics

Single `clk`, asynchronous active-low `rst_n`, `gpu` domain. Reset zeroes the
whole file and returns to idle.

## Input and output packet layouts

**Register file port**: `rf_we_i` / `rf_waddr_i` / `rf_wdata_i` in,
`rf_raddr_i` / `rf_rdata_o` out. Host writes are accepted only while the walk is
not running.

**Run control**: `clear_i` (zero the file), `start_i`, `busy_o`, `done_o`,
`status_o`.

**Instruction memory**: `pc_o` out, the six instruction fields in, plus
`instr_count_i`. A registered read, per the M10K rules.

## Backpressure rules

**None, and there is nothing to stall.** The walk owns the file for the duration
of a run and the host owns it otherwise; the two never overlap, so there is no
arbitration to get wrong.

## Memory ownership

**64 × 32 bits of flops** — 2,048 in total — plus the small instruction latch.

Flops rather than M10K because a 64-entry file with three read ports and one
write port does not map onto a block RAM without duplicating it, and at this
size the flops are the cheaper answer.

**MULTIPLIERS: ONE.** Measured 2026-08-23 on Quartus 17.0.2 against
5CSEBA6U23I7: this cone was **79 DSP blocks of 112** when each of ten op units
owned its own multiplier, and is **4** now that they share
`zhao_field_mul` — one signed 33×33 lane, input- and output-registered.

That is the DSP ruling of 2026-08-23 applied here: share only operations that
are mutually exclusive INSIDE the subsystem, and inside this one every operation
is mutually exclusive with every other, because the walk retires one instruction
at a time. `zhao_field_exec_shared` therefore holds one lane, one
`zhao_field_isqrt` (LEN and NORMALIZE), one `zhao_field_sin` (OP_SIN, OP_COS and
ROT's two reads), one `zhao_field_rcp` (OP_RCP and RING's two spans) and the two
reciprocal seed ROMs — which are different tables and are not interchangeable.

**No production op unit keeps a private nonconstant multiplier.** The only `*`
on a nonconstant pair in the Field cone is inside `zhao_field_mul`.

## Q formats and rounding

**None of its own.** Every value it moves is `fx16` and every rounding decision
belongs to the op block that made it.

## Latency (fixed or variable)

**Per instruction, and it now depends on the opcode.** Six clocks for anything
that finishes in `Q_EXEC` — fetch, latch, three operand-group reads, execute —
and longer for the ops that walk the shared lane. MEASURED by section 12 of
`tests/differential/field_seq_directed.cpp`, which prints the table on every run:

| op | clocks |
| --- | ---: |
| MOV, LDC, ADD, SUB, MUL, MAD, MIN, MAX, ABS, CLAMP, SELECT, CMP, DOT2, DOT3, SIN, COS | **6** |
| RCP | 15 |
| RIDGE | 22 |
| ROT2, ROT3 | 24–25 |
| DCURVE, CURVE | 26–29 |
| NOISE2 | 29 |
| LEN2, LEN3, DIST2 | 48 |
| SPLINE | 45 |
| RING | 54 |
| NORMALIZE2, NORMALIZE3 | 66–67 |

**MUL, MAD, DOT2 and DOT3 still cost six clocks on a machine with ONE
multiplier**, and that is the point of the schedule rather than a coincidence:
the three register-read cycles were idle, so they became issue slots, and the
first operand group is read in `Q_LATCH` from the instruction memory's own
outputs rather than a cycle later from the latched fields. With a two-cycle lane
that puts DOT3's third product in `Q_EXEC`, the state that consumes it.

`zhao_field_seq_pkg::MAX_OP_CYCLES` = **80** is the ceiling on one instruction.
It is not a comment: `tests/formal/field_seq_bound.sby` imports it and proves
`op_cnt <= MAX_OP_CYCLES` for an ARBITRARY instruction memory, and derives its
run-level bound and its BMC depth from the same constant.

Variable overall, since it depends on the program.

## Target throughput

Six clocks for a simple instruction; see the table above for the rest. This is a
per-sample field engine, not a per-pixel path, which is the whole reason a
NORMALIZE3 costing 67 clocks is a better trade than ten idle multipliers.

## Overflow and malformed-input behaviour

**THIS BLOCK DOES NOT VALIDATE, AND THAT IS THE DESIGN.**

`zfield::interpret` runs only on **decoded** programs — its `default:` case is
`__builtin_unreachable()`. The decoder is the validator, and it proves:

- every source register and every destination lane is in range, so **nothing
  wraps** here;
- no register is read before it is written;
- a destination never overlaps an input lane, and never overlaps its own
  sources — which is why the write-back needs **no bypass network**;
- there is exactly one `OP_END` and it is last;
- unused operand fields are zero, and every immediate is in range for its class.

Re-checking any of that here would be a **second implementation** of the rules,
and two implementations of a rule is how they drift apart. The one that runs on
untrusted bytes is the decoder.

**One thing is checked, and it is not a semantic check.** `instr_count_i` bounds
the walk. A lawful program never reaches it, but the instruction *memory* is the
shell's to load, and a walk with no bound turns a mis-loaded memory into a
machine that hangs forever rather than one that reports `ST_PC_OVERRUN`. A hang
is the worse failure and the one nobody can debug from a frame capture.

**An op outside the dispatch is REFUSED** with `ST_UNSUPPORTED_OP` and the run
stops. It is not skipped and it does not return zero, because a sequencer that
quietly ignores an opcode produces a plausible field and a wrong world.

## Counters and traces

`instr_retired_o`, one pulse per executed instruction, feeding
`field_instructions_by_profile`.

The SatLedger lanes — `sat_add_o`, `sat_mul_o`, `sat_rescale_o` — accumulate
across the **whole program**, exactly as the reference's single `SatLedger`
does, and are cleared at `start_i` rather than per instruction.

## Scalar reference function

`zfield::interpret` — `reference/src/zfield/zfield_interpret.cpp`.

The interpreter itself, not a paraphrase of it — the same answer
`design/ops.yml` gives, because `field-ir.md` §1 puts op semantics in exactly
two places and this block's law is one of them.

That choice is the reason this block found a defect nothing else had: see below.

## Directed tests

`tests/differential/field_seq_directed.cpp` — 102 checks, plus ~1,850 with
`--random 600`. The harness is the instruction memory and the host: it zeroes,
loads the input lanes, starts, and reads the output lanes back — the
reference's own order, which is part of the law.

Sections: a one-instruction program; the file starting at zero; dependency
chains up to 24 long; `OP_END` with live instructions after it; a ledger that
saturates early and must still report at the end; `DOT3`/`DOT2` adjacent-lane
reads; the `c` operand via `MAD`/`CLAMP`/`SELECT`; registers in the top half of
the file; a refused op; the liveness bound; and host writes attempted during a
run.

Mutation sweep, the walk itself: **19 mutations, 17 caught, 2 recorded
equivalent, 0 discarded.** A second sweep covers the unit dispatch — see below.

### The dispatch, and why there is no arbiter

Every opcode is dispatched into `zhao_field_exec_shared`, which owns all the
arithmetic and muxes it on the EXECUTING OPCODE. `OP_SIN` and `OP_COS` still
finish in `Q_EXEC` — the sine table is combinational, so they cost exactly what
an `ADD` costs. `OP_RCP` no longer does: its two products walk the shared lane,
so it became ready/valid like the rest.

**There is no arbiter and none is needed**, because the walk has exactly one
instruction in flight: an op is handed over in `Q_MISS` and drained in `Q_MWAIT`
before the next fetch, and the read slots finish issuing in `Q_RD2`, two cycles
before the earliest a multi-cycle unit can be accepted.

That fact used to be a scheduling convenience and is now the safety argument for
the whole engine, so it is tested as one rather than asserted. Section 13 of the
differential runs each operation ALONE, then in hostile sequences in both
directions, and requires every answer AND every one of the five saturation
ledger lanes to equal its isolated result — plus the same operation three times
in a row, which is what an accumulator that is added to rather than loaded
fails. The mutation sweep proves that section is not decoration: M05 makes
exactly that change and is caught.

**Two ledger lanes arrive with RCP.** `sat_rcp` is a genuine saturation and is
part of `Status.sat`, which the reference computes as
`add || mul || rescale || unit || rcp`. **`rcp0` is not** — it records that a
reciprocal was asked for zero, which has a DEFINED answer, and the reference
keeps it in its own field so a defined answer does not read as an overflow.
`diff()` checks the two separately, so a design that folded them together fails
rather than looking correct. That folding was a real defect in `RING`.

Tests: 305 directed (was 102) plus ~2,705 with `--random 600`. Section 7b covers
the quadrant boundaries, both rails, the zero case, an early `rcp0` still
reported at the end, `sin`/`cos` alternating to catch a selector latched from the
previous instruction, and a unit result feeding the next instruction.

**`sin` ignores the upper half of its register and does not reject it.** The law
is `angle16{(uint16_t)reg[a]}`, so rubbish above bit 15 must produce the same
defined answer the software gives. A design that fed the whole 32-bit register
to the ROM passes every quadrant test and fails that one.

Mutation sweep, the dispatch: **24 mutations, 21 caught, 3 recorded equivalent,
0 discarded** — attempted, expected and accounted all 24.

**The three equivalents are one fact about ANOTHER BLOCK.** `zhao_field_alu`'s
`default:` case sets `op_unsupported_o` and clears `writes_o` but leaves its
three saturation lanes at their block-initialised zero, so masking them for a
unit op is provably a no-op today. The mask stays anyway, because the redundancy
is a property of the ALU's default case rather than of this block, and depending
on another module's unstated behaviour is exactly how the `abs` defect below
survived weeks of green tests.

**The refusal test no longer pins itself to whichever op is unimplemented.**
Wiring RCP broke section 7, which had used `OP_RCP` as its example of an
unsupported op. It now tests both a real-but-unwired op (`OP_ROT3`, which will
break again when that lands, deliberately) and an opcode that is not in the enum
at all and never will be — the stable statement of the law.

**The two equivalents are a redundant PAIR, not dead code.** The write-back is
guarded by `alu_writes && !alu_is_end && !alu_unsupported`, and the ALU clears
`writes_o` for exactly `OP_END` and an unsupported op. So `alu_writes` and
`!alu_is_end` are individually redundant and each survives removal — but
`both_write_guards_removed` is **caught**, which is what shows the pair is
load-bearing. That mutation only became visible once a test named register 0 as
an output: `OP_END`'s `dst` is zero, and no earlier program ever read register 0
back.

### The defect this block found in another

`OP_ABS` in `zhao_field_alu` returned `INT32_MIN` for `abs(INT32_MIN)`. The
reference is explicit — `§3.7 saturating abs: abs(0x80000000) = 0x7FFFFFFF + SAT`
— and returns `INT32_MAX`, bumping the `rescale` lane.

The ALU's own test restated the law **the same wrong way**, and even carried a
comment saying "the oracle and the RTL could agree and both be wrong about what
the reference does with this one input" before asserting the wrong value. So the
two agreed with each other and the reference was the odd one out.

Nothing caught it until this block's differential ran whole programs through
`zfield::interpret` itself. The ALU now saturates, has the `rescale` lane it was
missing, and its test asks the shipped interpreter rather than a paraphrase.

**That is the argument for testing against the real oracle wherever it can be
reached**: a restatement can be wrong, and a wrong restatement agrees with a
wrong implementation forever.

## Randomized differential tests

The `--random` lane builds random straight-line programs — random length,
random ops from the arithmetic set, random inputs — with the dependency
structure the decoder requires, and compares every output lane and the collapsed
`Status.sat` against the interpreter.

## Formal properties

None yet. The set worth proving: the walk always terminates (either at `OP_END`
or at the bound); `done_o` implies the file is stable; a host write is never
accepted while `busy_o`; and `pc` only ever advances by one.

## Synthesis / resource ceiling

Not yet fitted. 2,048 flops of register file plus three 64:1 read muxes is the
dominant cost, and the mux depth is the reason there are three ports and not
seven.

## Integration capture cases

None yet. The first is a real `.zprog` from the compiler, run end to end through
`FIELD.PROGCACHE` into this block — which needs the remaining op dispatch first.
