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

## Q formats and rounding

**None of its own.** Every value it moves is `fx16` and every rounding decision
belongs to the op block that made it.

## Latency (fixed or variable)

Six clocks per instruction: fetch, latch, three operand-group reads, execute.
Variable overall, since it depends on the program length.

## Target throughput

One instruction per six clocks. This is a per-sample field engine, not a
per-pixel path.

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

Mutation sweep: **19 mutations, 17 caught, 2 recorded equivalent, 0 discarded.**

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
