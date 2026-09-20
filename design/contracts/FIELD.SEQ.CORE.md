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

## AMENDED 2026-08-27 — one semantic engine, profile adapters permitted (Field v3)

Ruling from `reports/Fieldv3.md` (Phase 1), superseding nothing about the
semantics and everything about which machine runs them in production:

**One SEMANTIC engine.** The canonical ISA (`spec/form/field-ir.md`), its
validator, its program hash and `zfield::interpret` remain the single law.
Three RTL realisations of that law now exist or are ruled, and their statuses
are part of this contract:

| engine | files | status |
|---|---|---|
| v1 scalar walker | `zhao_field_seq.sv` + units | FROZEN — exact serial reference and differential oracle (the machine the body of this contract describes) |
| v2 SIMD barrel | `zhao_field_v2_core.sv` / `_front.sv` / `_lanemux.sv` | FROZEN — exact fallback and differential reference RTL; **not** the Earth60 production path (measured: 59.22 MHz restricted Fmax, −6.886 ns setup / **−1.938 ns hold** at 100 MHz, 27,225 transport clocks/association, 439–930 % of the reserved budget even at a hypothetical 100 MHz — see the ruling block in `zhao_field_v2_core.sv`) |
| v3 prepared vector fabric | to be built (Phase 3 probes first) | the production path: FPLAN-prepared programs, ready-context FIFO scheduler, four-wide vector execution, queued long-op services, field-major patch reduction |

**Profile adapters are PERMITTED and are the v3 front end.** A profile is a
program set plus a STREAM ADAPTER — a small generator that produces the
varying input lanes directly (Earth: lattice x,z from patch origin + pitch;
Warp: vertex position/normal; Flow: particle state; Formation:
instance/index; Stamp: stencil u,v) and consumes the outputs directly from
snooped export registers. Adapters generate and consume streams; they NEVER
re-implement an op, and there are not five engines. The generic
12-in/4-out per-point host transport of the v2 front is measured off the
production path for good (261 % of the reserved frame before one instruction
executes).

**FPLAN is a derived artifact, not a second ISA.** A canonical program is
lowered by the exact software planner into vector uops + a prepared uniform
block; the canonical→uop translation is GENERATED from the canonical
operation table, never hand-written per opcode — the v2 private encoding that
collides with canonical values (`tests/differential/field_v2_front_directed.cpp`,
`to_ref()`) is the failure class this rule exists to kill. A canonical
program without an FPLAN still runs on the frozen engines or in software; it
just carries no Earth60 certificate.

## Clock and reset semantics

Single `clk`, asynchronous active-low `rst_n`, `gpu` domain. Reset zeroes the
whole file and returns to idle.

## Input and output packet layouts

**Register file port**: `rf_we_i` / `rf_waddr_i` / `rf_wdata_i` in,
`rf_raddr_i` / `rf_rdata_o` out. Host writes are accepted only while the walk is
not running.

> **The read is SYNCHRONOUS as of 2026-08-24.** `rf_raddr_i` is presented on one
> edge and `rf_rdata_o` answers on the next. It used to be combinational, and
> the change is a consequence of the register file becoming block memory: it was
> 2,048 bits of flops behind four asynchronous 64:1 muxes — which this contract
> already named as the dominant cost — and an asynchronous read is one of the
> three things `reports/QUARTUS_GOTCHAS.md` §10 measured as independently fatal
> to memory inference.
>
> The symptom of missing this is unambiguous and was seen: **every chained result
> reads back as the PREVIOUS instruction's answer.** A reader that settles
> combinationally and does not clock will see stale data, not garbage.
>
> **A consequence for observers**: a testbench watching a register *during* a run
> must hold `rf_raddr_i` and read the port, not call a helper that clocks — an
> extra edge per observation swallows `instr_retired_o` pulses and reports work
> as lost. And an observation now lags the file by one cycle, so a check timed
> against a retire pulse needs to skip the first sample after it.

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

**SEVEN clocks as of 2026-08-24, not six** — the register file became block
memory, so a read is answered one edge after its address, every operand group
lands a state later, and a fourth read state (`Q_RD3`) catches the last one. The
sentence below is kept because its ARGUMENT is unchanged and is the point: the
count moved by one, the reason it is small did not.

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

### AN ABSENT OUTPUT IS A REFUSAL, NOT A ZERO (owner ruling R101, 2026-09-20)

The paragraph above is about an op the machine will not run. This is the same
law one level up, about a LANE the program did not produce, and it was broken
in `zhao_field_host` — the shell every profile shares — from the day the front
was written until 2026-09-20.

`zhao_field_host` captures a run's result by watching the register-file write
port over a CONTIGUOUS window `[out_base, out_base + OUT_LANES)`, clearing the
window to zero at grant. Its completion test was `cur_out_seen == '0`: **did ANY
lane get written**, not did all the required ones. A point that wrote five of
its six declared lanes therefore answered `8'h00` SUCCESS with the sixth lane
carrying the zero from the clear — a plausible field and a wrong world, exactly
as above, and `GEOM.WARP`'s law W10 in as many words: *"Do not make an absent
output look like a zero result."*

**The defect was upstream of the guard.** "Required" was not a quantity the
loader interface carried, so `cur_out_seen == '0` was the best test available
from the information present, and the block's own comment states the hazard
correctly before guarding only the all-zero case. Blaming the guard sends the
next person to the wrong line.

**The law now.** The program header word carries a **required-output mask** in
bits `[32 +: OUT_LANES]` — 64 bits there are unread by the header decode, which
touches only `[8 +: REGW]` and `[22:16]`. It is `spec/form/field-ir.md` 7.1's
per-profile output record (earth 4, warp 6, flow 7, formation 6, stamp 3),
transported; it is not a new law. Completion is then:

| `cur_out_seen` | verdict | status | counter |
|---|---|---|---|
| `== 0` | wrote nothing | `ST_NO_RESULT` 0xF1 | `no_result_o` |
| `!= 0`, `(seen & mask) != mask`, `mask != 0` | **wrote some, not all** | `ST_PARTIAL` 0xF3 | `out_incomplete_o` |
| otherwise | complete | `8'h00` | `runs_o` |

`mask == 0` means "the program declared nothing" and keeps the previous test
**exactly**, so the repair is additive and nothing written against the old
contract changes meaning.

**HOLES ARE THE NORMAL CASE, NOT AN EDGE ONE, AND THAT IS THE MEASUREMENT THAT
JUSTIFIES THE MASK.** The capture window is contiguous; the IR does not require
a program's output registers to be. `tools/field/zprog_output_coverage.py` is
the committed probe, and on the three real Earth programs this repo ships:

| program | output regs | span | mask | holes at `OUT_LANES=7` |
|---|---|---|---|---|
| `crater_ring` | R13,R14,R15,R17 | 5 | `0x17` | lanes 3, 5, 6 |
| `impact_wave` | R12,R14,R15,R16 | 5 | `0x1D` | lanes 1, 5, 6 |
| `wave_pool`   | R12,R13,R14,R16 | 5 | `0x17` | lanes 3, 5, 6 |

Every one writes all four outputs its profile declares, and every one leaves
three lanes of the console's seven-lane window unwritten. Under the old test all
three answer SUCCESS with three cleared zeroes in the result. (They also need a
five-lane span, so none of them fits the module's default `OUT_LANES=4` at all —
one declared output falls outside the capture window entirely.)

**What this does NOT close.** The capture is still ONE CONTIGUOUS WINDOW. The
sparse output map that `GEOM.WARP` prerequisite P3 asks for is not built; the
mask lets the host *refuse* an undeclared-but-required lane, which removes the
silent zero, and does not let it capture a lane outside the span.

Fired by legal stimulus, not argued: `tests/field/field_host_directed.cpp`
cases 1b and 1d, the latter running one program twice with only the mask moved.

### SUPERSEDED 2026-09-20 BY THE ORDINAL LAW — `zhao_field_host_v2` (packet H1)

The section above is **correct and insufficient**, and the sentence that says so
is already in it: *"the capture is still ONE CONTIGUOUS WINDOW."* R111 then
measured what that costs and the answer was not an edge case. This section is
the law `zhao_field_host_v2` implements. `zhao_field_host` is **retained,
unchanged, as the named oracle (FH02)**; it appears in `tests/CMakeLists.txt`
and in **no production source list**, because composing both versions is exactly
what `completion_register.py`'s `superseded_in_closure()` exists to catch.

**ORDINAL IS NOT WINDOW, AND THEY ARE DIFFERENT TYPES.**

| quantity | indexed by | width | lives in |
|---|---|---|---|
| **window mask** | contiguous capture position `k`: register `out_base + k` was written | `OUT_LANES` (7) | header `[32 +: OUT_LANES]`, `zfh_window_mask_t` |
| **required mask** | canonical output **ordinal** `j`: output `j` is declared | profile output count, carried in a u8 | `PROGRAM_META.required_mask`, `zfh_required_mask_t` |

They coincide only when a program's output registers run contiguously from
`out_base`, and the table above measured that **they never do**. The generated
schema gives them distinct type names and distinct widths so a cross-assignment
is a compile error rather than a silent truncation, and `OUTPUT_MAP` — one row
per ordinal, carrying `source_kind` and `source_index` — **is the translation.
Nothing else converts between the two spaces.**

A window mask can detect a missing write. It **cannot** compact window slots
`0,1,2,4` into canonical result slots `0,1,2,3`, so nothing indexed by window
position can return results in declared output order. `resp_out_o` is therefore
`OUT_ORDINALS` wide and ordinal-indexed; `resp_window_o` is the only
window-indexed port on the module and exists solely so a caller can tell
**padding** (a window lane no ordinal claims) from a **missing result**.

**COMPLETION IS `ALL`, PER ORDINAL, ON NEXT-STATE SETS.**

```
seen_next = seen | writes_granted_this_clock | valid_uniform_seeds
complete  = ((seen_next & required_mask) == required_mask) && fenced
```

`seen[j]` is set by a **granted** register write whose register equals
`source_index[j]`, so two ordinals naming one register are both satisfied by one
write. Only the arbiter's granted write counts; a refused ALU request is not a
committed output.

**A UNIFORM OUTPUT IS A RESULT.** For `source_kind == PREPARED_SCALAR` the
export is **seeded at point start** and `seen[j]` set there, under two conditions
that are checked and never inferred from the data:

* the prepared slot's **valid bit**, which is driven independently of the value —
  a prepared zero with its valid bit set is a result, a zero without it is an
  absence;
* the slot's **preparation generation** against the running association's. A
  matching program hash is not a matching parameter set.

An all-uniform image is declared `UNIFORM_ONLY` and **does not start a context at
all**: it has zero physical uops, so starting one would park it waiting for an
END no instruction will issue. The form is not taken on trust — an image
declaring `UNIFORM_ONLY` whose declared ordinals are not all prepared scalars is
refused at load.

**END IS NOT A FENCE.** Publication waits for a real drain, not a guessed delay.
The oracle computes its verdict on the clock END is observed, reading the
pre-update `seen` register, and stops capturing there; `E_DRAIN` keeps capturing
and the verdict is computed on next-state sets. The fence condition is
over-determined from the four things a host can see — `active_o == 0`,
`!dbg_long_valid_o`, and `rf_writes_o`/`drain_writes_o` stable for a clock —
because **there is no per-context outstanding-work port anywhere in the v3
fabric**. That was searched for, not assumed: at port level the only matches for
`*_outstanding*`, `*_inflight*`, `*_pending*`, `*_busy*`, `*_drain*`, `*_empty*`
across the whole family are `idle_clocks_o` and `drain_writes_o`, and both are
32-bit **counters**. `active_o == 0` alone is already sufficient *by
construction*, and that is precisely why it is not used alone: the sufficiency is
**emergent from three gates in three different files**
(`zhao_field_v3_exec.sv:378`'s `!sk_busy_c`, `zhao_field_v3_dispatch.sv:694`'s
release-with-last-write, `zhao_field_v3_exec.sv:1373`'s un-park), declared
nowhere and covered by no assertion. `late_write_o` is the fence's own instrument
and must read zero.

**LOAD-TIME REFUSALS.** The header is written **last** and is the write that
makes a slot runnable, so it is also the first moment the descriptor is
**complete** — and therefore the only place it can be validated. Four refusals
live there, each with its own counter:

| condition | counter |
|---|---|
| ordinal mask is zero while `output_count` is not (R111's hazard) | `zero_mask_o` |
| `output_count` disagrees with the mask's population count | `bad_image_o` |
| `UNIFORM_ONLY` with a declared ordinal that is not a prepared scalar | `bad_image_o` |
| a `VECTOR_REG` ordinal outside `[out_base, out_base + OUT_LANES)` | `bad_image_o` |

The last one is the case that otherwise produces a wrong value silently: the
write happens, the window never observes it, the ordinal can never be seen, and
the point refuses for the **wrong reason** — sending the next reader to the
completion logic instead of to the image.

**THE PER-POINT CLEAR IS SKIPPED ONLY UNDER A PROOF.** `hdr_ipok[slot]` is set
only by an accepted `INIT_PROOF` load and is the sole gate on the fast path;
`cfg_slow_clear_i` forces the walk back on for the differential. The proof is a
property of the image and is walked by the C++ validator — the hardware owns the
interlock, not the proof.

**NUMERIC STATUS IS FOUR CAUSES.** `num_status_o` is
`{rcp0, sat_rescale, sat_mul, sat_add}` where the oracle's `sat_o` is three bits.
`rcp0` keeps its own family, per directive §8.1's `numeric_rcp0` separate from
`numeric_sat`: a reciprocal of zero is a **defined answer**, not a saturation and
not a fault, and folding it into either is one of the two wrong things to do with
it. **Its producer chain was incomplete when this contract was written and is
complete as of packet C1, 2026-09-20 (owner ruling R145).** `rcp0` now has a port
on `zhao_field_v3_svcpath` (`svc_rcp0_o`), `_dispatch` (`rsp_rcp0_i` /
`svc_rcp0_o`) and `_engine` (`rcp0_o`), carried along the `sat_rescale` template
and masked to the group's live lanes by the same `s_used_r`.

**It is THREE files and not four, and the difference is a tie-off avoided.**
R145 names `_core` as the fourth. `zhao_field_v3_core` and `zhao_field_v3_exec`
contain no `rcp0` signal at all — measured by grep across `fpga/rtl/field/`,
zero hits in both — because the reciprocal that can be handed a zero lives in
`zhao_field_v3_normalize`, on the **service** path, never under core. A `rcp0_o`
on core added for symmetry would have been a port with nothing driving it.

`rcp0_i` is **retained** on this block rather than removed. It existed so that
connecting it would be a wiring act with a visible unconnected end; now that the
end is connected it remains the bench's way to force the cause without reaching
inside the fabric, and the run accumulator ORs it with the fabric's own bit. The
console drives it low, so on the composed machine `fab_rcp0` is what reports the
event. `zhao_field_host.sv` — the retained oracle — leaves the engine's new
output explicitly unconnected, because its `sat_o` is `[2:0]` and widening the
oracle to match would be changing the thing that exists to check this block
independently.

**ONE ACTIVE PREPARED-DATA DOMAIN (FH09), AND ITS HONEST LIMIT.** The prepared
file carries a generation tag per slot and a read whose tag does not match the
running association refuses rather than returning another association's number.
That is one *active* domain with exclusive ownership — **not** per-context
uniforms. It satisfies `GEOM.WARP` prerequisite **P5 only if Warp never
interleaves with Earth inside a frame**, so P5 is **deferred with a measured
justification, never closed.**

**Evidence.** 152 checks in `tests/field/field_host_v2_directed.cpp`, built and
run. R126's elaboration guard **seen to fire** by `field_host_v2_r126_guard` —
necessary because `--lint-only` does not run `initial` blocks, so a clean lint is
no evidence about it. R101's ANY-not-ALL defect re-planted as
`tests/mutants/zhao_field_host_v2_any_not_all_mutant.sv`, one substantive line,
**fired**, with case 1d polarity B as its negative control. `fieldp4`'s case 1d
is carried forward into ordinal space as the permanent both-polarity control.
**PHYSICAL FIT PENDING.**

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
