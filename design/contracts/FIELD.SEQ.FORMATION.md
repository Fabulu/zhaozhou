# Contract — FIELD.SEQ.FORMATION (Formation field sequencer)

> Ledger: `design/blocks.yml` · owner ZH-041 · phase 9 · maturity SPECIFIED

## Purpose and exclusions

Evaluate transform-generation programs feeding the Transform Loom; implements the M profile of ops.yml.

## This is a PROFILE, not a block

Owner ruling, 2026-08-22: **one engine, five profiles.** This contract
describes a CONFIGURATION of `FIELD.SEQ.CORE`, which is a complete Field IR
sequencer and is already `RTL_VERIFIED`. There is no separate
`FIELD.SEQ.FORMATION` sequencer in hardware and there is not going to be one.

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

**What is still open, and it is not hardware.** The `M` profile of
`ops.yml` still needs its lane binding written down: which registers the
inputs arrive in and which the outputs are read from, per program. That is a
software and shell question, and it belongs with the blocks that consume the
output.

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
different set of I/O lane bindings. This block evaluates transform-generation programs feeding the Transform Loom — the M profile of
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
