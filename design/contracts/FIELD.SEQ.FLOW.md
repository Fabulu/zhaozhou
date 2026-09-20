# Contract — FIELD.SEQ.FLOW (Flow field sequencer)

> Ledger: `design/blocks.yml` · owner ZH-042 · phase 10 · maturity SPECIFIED

## Purpose and exclusions

Evaluate particle/force-field programs driving Myriad updates; implements the F profile of ops.yml.

## This is a PROFILE, not a block

Owner ruling, 2026-08-22: **one engine, five profiles.** This contract
describes a CONFIGURATION of `FIELD.SEQ.CORE`, which is a complete Field IR
sequencer and is already `RTL_VERIFIED`. There is no separate
`FIELD.SEQ.FLOW` sequencer in hardware and there is not going to be one.

The ledger records this as `kind: profile` with
`implemented_by: FIELD.SEQ.CORE` (rule V21), which is why this entry carries
no `reference_model`, no directed or random test of its own, and no ALM
budget: the engine carries all three, and counting them again here counted
one engine five times.

**Why the five were never distinguishable in hardware.** `zhao_field_seq` has
no profile input and no profile-specific port. The thing that would
distinguish a profile -- which registers the input and output lanes bind to --
is carried by the DECODED PROGRAM (`zfield::Decoded::in_lanes` /
`out_lanes`, filled by the decoder from the image), not by the block. So a
profile is a program set plus shell wiring, not a hardware variant.

**The `F` lane binding, WRITTEN DOWN.** This section used to say the binding
"still needs writing down". It is settled by two ratified documents and this
contract only records them.

### The canonical F record

`spec/form/field-ir.md` §7.1 line 524, verbatim:

| Profile | id | Input record (R0..) | Output record |
|---|---|---|---|
| flow | 2 | px,py,pz, vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx (13) | px′,py′,pz′, vx′,vy′,vz′:fx, attr0:fx (7) |

**Thirteen canonical inputs, seven canonical outputs.** That is the SEMANTIC
ARITY and it is a different quantity from the PHYSICAL CLIENT PORT (FH17).
Flow is the widest OUTPUT profile, which is why the shared bus is composed at
7 out; Warp is the widest INPUT profile, which is why R103's prerequisite P1
moves the input half from 13 to 15.

**Output lanes are read by CANONICAL ORDINAL, never by physical
capture-window position.** `zhao_field_flow_adapter` reads ordinals 3, 4 and 5
— `vx′, vy′, vz′` — whatever register the program wrote. R111 named this exact
adapter as the case that made the hazard concrete: the shipped Earth programs
leave three of seven window lanes unwritten, and a program writing only lane 6
"would have fed flow three zero velocities, a plausible-looking maximum
deceleration". The window→ordinal compaction is the HOST's act through
`OUTPUT_MAP`.

### R40's seam onto PART.UPDATE

Owner ruling R40 (provisional; particle MOTION is art, so the owner judges by
eye):

```
acceleration = sat_s11((v' - v) >> 8)
seed         = the variation byte
dt           = 1 tick
```

Every value R40 names is a NAMED, EDITABLE PARAMETER of the adapter —
`ACC_SHIFT`, `ACC_ROUND`, `DT_FX`, `POS_SHIFT`, `VEL_SHIFT` — so the owner's
revision is a parameter edit at the composition (CLAUDE.md rule 6).

**THREE OF THE SEVEN OUTPUTS ARE READ, AND THAT IS THE RULING RATHER THAN AN
OVERSIGHT.** R40 maps this seam onto PART.UPDATE's ACCELERATION port and
PART.UPDATE integrates position itself from the velocity it owns. Taking the
program's `px′/py′/pz′` would be **a second integrator with a different
rounding law running beside the ratified one**; `attr0` has no port on that
block at all. Directive §15.1 requires this exclusion be listed honestly —
"Return all seven canonical Field outputs correctly even though the
application uses only three of them" — and it is listed here.

### §15.1's CAPTURE RULE, and the defect it named

Directive §15.1: "Latch parameters, origin, dt, frame and the actual particle
record at request capture. **Derive no later result from unrelated live
`rec_i` or `par_i` pins.**"

**That named a defect that was live in this tree until 2026-09-20.** The
adapter drove `req_in_o` combinationally from the live pins, and R40's
SUBTRAHEND `in_vx/in_vy/in_vz` was read from those same live pins at RESPONSE
time — tens of clocks after the run began. A record moving mid-flight
therefore produced

```
acceleration = (v' OF RECORD A) - (v OF RECORD B)
```

a well-formed, plausible, entirely wrong wind. The identity guard beside it
could not prevent it: `rec_changed_o` is sampled one state AFTER the wrong
difference is latched, and a record that moved during the wait and moved back
before the answer was offered never fired it at all. **A detector downstream
of the corruption is not a guard.**

There is now ONE capture latch, taken at the cycle the run is decided. The
offer is driven from it, so a standing request cannot have its operands moved
underneath it; and R40's subtrahend is read back out of it, so **the two sides
of the subtraction are the same point by construction rather than by timing
argument.** Parameters, origin and dt ride in the same latch because §15.1
names all of them. Cost: `IN_LANES*32` = 416 flops. PHYSICAL FIT PENDING.

### The identity guard, and why it still earns its place

`rec_changed_o` remains, and it remains REACHABLE: its two operands are
clocked by different things — `held_rec` is captured once at request time,
`rec_i` is the live wire — which is the first question CLAUDE.md's
metadata-swap chapter says to ask of any checker. It is fired by stimulus in
`tests/field/field_flow_adapter_directed.cpp`. What changed is that it is no
longer the only thing standing between a moved record and a wrong
acceleration; it is now a REPORT that the producer misbehaved, not the
mechanism that keeps the arithmetic correct.

### Absent is not zero

`fld_valid_o` LOW is the honest answer when no FLOW program is resident, when
the engine refuses the run, or when the run raises an alarm. PART.UPDATE adds
the sample only `if (fld_valid_i)`, so a low valid REMOVES the term rather
than adding a zero one. Each reason is counted separately — `bypassed_o`,
`noprog_o`, `faults_o` — because a merged "no sample" total cannot tell an
unarmed console from a broken program. §15.1: "Numeric sat/rcp0 does not turn
a valid Field result into the no-field bypass."

### Still open, and it is NOT built here

§15.1 permits buffering — "A record queue can allow several independent
particles into the host while retaining the ordered original records for
PART.UPDATE" — and requires that if it is added, **both handshakes are named**:
`rec_take_i` means "the update consumed this record's answer" and must never
be reused to mean "the upstream record was captured".

**No queue is built.** `zhao_field_host`'s front holds ONE point in flight, so
a deeper queue here would be a mode for a stage that does not exist, and it
could not be tested against the real host. The capture latch above is its
prerequisite and is the half that was genuinely wrong. `stall_cycles_o`
measures what the scalar front costs. Building the queue belongs with the
host's credit work (FH20).

The sections below are the generated stubs. They are kept rather than deleted
because a profile still has an I/O contract at the shell boundary -- but they
describe a configuration of the engine, never a second engine.

---

## Clock and reset semantics

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Input and output packet layouts

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Backpressure rules

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Memory ownership

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Q formats and rounding

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Latency (fixed or variable)

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Target throughput

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Overflow and malformed-input behaviour

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Counters and traces

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Scalar reference function

`zref::fieldir::interpret` — `reference/include/zref/zref_fieldir.hpp`, which
forwards to `zfield::interpret` in `reference/src/zfield/zfield_interpret.cpp`.

**The same interpreter FIELD.SEQ.CORE is measured against, and deliberately so.**
A profile is not a different machine: it is the core sequencer wearing a
different set of I/O lane bindings. This block evaluates particle/force-field programs driving Myriad updates — the F profile of
`design/ops.yml` — but the op semantics it runs are the interpreter's, not a
second statement of them. `field-ir.md` §1 puts op semantics in exactly two
places and this is not a third.

What is profile-specific is the LANE BINDING: which registers the input lanes
land in and which the output lanes are read from. That is what a directed test
for this block has to pin, and it is why the block needs its own test rather
than inheriting CORE's.

## Directed tests

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Randomized differential tests

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Formal properties

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Synthesis / resource ceiling

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Integration capture cases

**Not a hardware contract.** Owner ruling 2026-08-31 section 8: the FIELD.SEQ.* entries are **Field IR PROGRAMS, not datapaths**. The engine they run on is built and heavily optimised; authoring one of these is closer to CONTENT than to hardware, and there is no separate block to give clocks, packets or a throughput target. These sections stay unwritten on purpose -- filling them would describe an engine that already exists elsewhere, under a second name.

## Notes

Planning split; may share the Field ALU with EARTH post-synthesis (§6A).
