# Contract — FIELD.SEQ.STAMP (Scar Scribe stamp sequencer)

> Ledger: `design/blocks.yml` · owner ZH-045 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

Evaluate Scar Scribe sheet programs (stamp placement/conversion); implements the S profile of ops.yml.

## This is a PROFILE, not a block

Owner ruling, 2026-08-22: **one engine, five profiles.** This contract
describes a CONFIGURATION of `FIELD.SEQ.CORE`, which is a complete Field IR
sequencer and is already `RTL_VERIFIED`. There is no separate
`FIELD.SEQ.STAMP` sequencer in hardware and there is not going to be one.

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

**The `S` lane binding, WRITTEN DOWN 2026-09-20.** This section used to say the
binding "still needs writing down ... it belongs with the blocks that consume
the output". It now belongs here, because owner directive
`reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt` §15.2 ruled
that there are **two** of them and that the difference between them is not a
detail.

### The canonical S record

`spec/form/field-ir.md` §7.1 line 526, verbatim:

| Profile | id | Input record (R0..) | Output record |
|---|---|---|---|
| stamp | 4 | u,v:unit, age:u32, strength:unit, p0..p3:fx (8) | tag_op:u32, strength:unit, emissive:unit (3) |

**Eight canonical inputs, three canonical outputs.** That is the SEMANTIC
ARITY, and it is a different quantity from the PHYSICAL CLIENT PORT the
adapter is wired to (13 in / 7 out as the console composes it, sized by the
widest composed profile). FH17 requires the two to be "separately declared";
`zhao_field_stamp_adapter` declares them as `IN_LANES`/`OUT_LANES` and
`S_CANONICAL_INPUTS`/`S_CANONICAL_OUTPUTS` and checks one against the other at
elaboration.

**Output lanes are read by CANONICAL ORDINAL, never by physical capture-window
position.** Ordinal 0 is `tag_op`, 1 is `strength`, 2 is `emissive`, whatever
register the program actually wrote. The window-to-ordinal compaction is the
HOST's act through `OUTPUT_MAP` (directive §7.1/§7.3). R101's window mask and
FH05's required mask are different named fields with different widths and are
never assigned to one another — R111 measured that the shipped programs'
output registers are never contiguous, so **holes are the normal case**, and an
adapter reading a window POSITION would be reading whichever register happened
to sit at that offset.

### TWO NAMED BINDINGS (FH26), and neither is reachable by omission

Directive §15.2: the existing adapter "drives two raw integer texel indices,
zeros other transport lanes, reads tag_op and a 16-bit strength, and describes
that as the S record ... **These are not identical contracts.**"

`zhao_field_stamp_adapter`'s `STAMP_BINDING` parameter selects between them:

| value | name | inputs | output ordinal 1 | ordinal 2 |
|---|---|---|---|---|
| 0 | `STAMP_BINDING_UNBOUND` | — | — | **refused at elaboration** |
| 1 | `STAMP_BINDING_LEGACY_BRUSH` | R0/R1 = RAW integer texel indices | the LOW 16 BITS | held at zero |
| 2 | `STAMP_BINDING_CANONICAL` | R0/R1 = UNIT texel centres | the §15.2 conversion | converted and exported |

**There is deliberately no safe default.** FH26 is "legacy is an explicit mode,
never the strict default", and a parameter whose default quietly selected the
permissive bridge would be the R111 shape exactly — a mechanism that reads as
protection while providing none. A composer that omits the binding gets a loud
elaboration failure naming both legal values. `zhao_console_core.sv` states
`.STAMP_BINDING(1)` and `design/prod_manifest.yml`'s
`production_parameter_overrides` states the same for the generated census top.

**The legacy bridge passing is NOT evidence that the canonical binding works.**
The two are separately elaborated in `tests/field/tb_field_stamp_bindings.sv`
and separately driven, and the directed test asserts that exercising one leaves
the other's counters at rest, in both directions.

### The case that discriminates them

One identical engine response carrying unit strength **fx16 1.0 =
`0x0001_0000`** on canonical ordinal 1:

* `LEGACY_STAMP_BRUSH` delivers `fld_strength_o = 0` — **no brush at all**,
  because the low 16 bits of fx16 1.0 are zero;
* `CANONICAL_STAMP` delivers `fld_strength_o = 65535` — **a full brush**.

That is the failure §15.2 names in so many words, and it is the maximum
possible disagreement between the two contracts from one response. On the input
side the same texel offers `R0 = 0` under legacy and `R0 = 512` under
canonical.

### The two canonical conversions (§15.2, written out because it writes them out)

1. **Texel centre, on the way in.** "a 64-wide unit center `(2*i+1)/128` maps
   exactly to fx16 `(2*i+1)*512`". Generalised and divider-free:
   `u_fx = (2*i + 1) << (15 - log2(SHEET_W))`, which requires the sheet to be a
   power of two — checked at elaboration. Column 0 is **512, not 0**, and
   column 63 is **65024, not 65536**: a texel centre is never on a sheet edge.

2. **Unit → u16, on the way out.**
   `u = clamp(signed_fx16, 0, 65536)`;
   `strength16 = floor((u * 65535 + 32768) / 65536)`, with a 34-bit
   intermediate. `u * 65535` is evaluated as `(u << 16) - u` so no DSP is spent
   on a constant one below a power of two. Mapping: `0 → 0`,
   `0x0001_0000 → 65535`, `0x0000_8000 → 32768` exactly, and anything outside
   the interval clamped with `canon_clamps_o` moving. The endpoint 1.0 is IN
   range and is not a clamp.

`tag_op`'s interpretation is **identical in both bindings** and is the
consumer's: `zhao_surface_stamp` unpacks tag = `[7:0]`, blend = `[10:8]`,
age_shift = `[14:12]`, and the adapter touches none of those fields.

### The honest exclusion

`emissive` is canonical ordinal 2. The canonical binding computes, converts and
**declares** it; `zhao_surface_stamp` **has no emissive input** — its `fld_*`
group is exactly `fld_valid_i`, `fld_ready_o`, `fld_tag_op_i`,
`fld_strength_i`, searched rather than assumed — so `zhao_console_core` leaves
`fld_emissive_o` explicitly unconnected with the reason beside it.

**And the consumer already recorded WHY, which is stronger than "not wired
yet".** `zhao_surface_stamp.sv:115-117`, verbatim:

> The `emissive` output lane of field-ir 7.1 is DROPPED: layer F has two bytes
> and charter 12 spends both, and adding a third would change the frozen
> 8,192 B layer size.

So this is a **ratified layout decision with a cost attached**, not an
oversight. Connecting the lane would mean reopening charter 12 and the frozen
layer size — an application decision, not an adapter one.

§15.2: "Do not claim the surface renderer uses the emissive output if it does
not. Retaining and correctly returning it is shared-host conformance;
commissioning its eventual surface effect is a separate application policy."
Inventing a console boundary output for a lane nothing reads would convert an
honest exclusion into a disconnected register entry.

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
different set of I/O lane bindings. This block evaluates Scar Scribe sheet programs (stamp placement/conversion) — the S profile of
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
