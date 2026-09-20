# The ZFH2 host-image envelope

**Status: frozen for image revision 1.** This is the physical transport format
for a FIELD program or association as it is installed into the shared host. It
is **separate** from the frozen `.zprog` format (`spec/form/field-ir.md` section
5) and from any game-facing command record (`spec/commands.zidl`). Changing a
byte here does not move `ZHAO_ZIDL_SHA256` and does not regenerate a single
golden capture.

Adopted from the owner directive
`reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt` sections
11.1 through 11.5 (owner commit `6262868c`), decision **FH2**.

**Every offset in this document is generated.** The schema lives in
`tools/field/gen_field_host_schema.py` and is emitted into three places:

| Artifact | Purpose |
|---|---|
| `reference/include/zfield/generated/zfield_host_image.hpp` | the C++ packer, validator and reader |
| `fpga/rtl/field/generated/zhao_field_host_image_pkg.sv` | the hardware loader and install checks |
| the generated region of this file | the human-readable law |

`python tools/field/gen_field_host_schema.py --check` is the freshness gate and
also runs the cross-checks described under **Instruments** below. The
directive's instruction is verbatim: *"Generate C++/SV constants and
packer/reader tests from one schema. Do not hand-maintain offsets in three
languages."*

---

## 1. THE TWO MASKS ARE NOT THE SAME NUMBERING

**This section is the reason the file exists. Read it before wiring anything.**

There are two bitmasks in the output path. They are both small, both called
"the mask" in conversation, and they are indexed by different things.

### `window_mask` -- indexed by CONTIGUOUS CAPTURE-WINDOW POSITION

`zhao_field_host.sv:854` computes

```systemverilog
wire [OUTW-1:0] out_idx_c = OUTW'(int'(fab_wr_reg) - int'(hdr_outbase[cur_slot]));
```

valid while that difference is less than `OUT_LANES`. **Bit *k* means
"physical register `out_base + k` was written".** Its width is `OUT_LANES`,
composed at 7 in `zhao_console_core`. It arrives in the loader header word at
`[32 +: OUT_LANES]` and lands in `hdr_outreq` (`zhao_field_host.sv:585`,
`:1074`). It is R101's repair, and it is correct for what it does: it can
detect a missing write.

### `required_mask` -- indexed by CANONICAL OUTPUT ORDINAL

**Bit *j* means "canonical output *j* of the profile is declared".** Its
meaningful width is the profile's output count -- earth 4, warp 6, flow 7,
formation 6, stamp 3 (`spec/form/field-ir.md` section 7.1) -- carried in a `u8`
in `PROGRAM_META`.

### They coincide only when output registers are contiguous from `out_base`

**And they never are.** `tools/field/zprog_output_coverage.py` measured the
three shipped Earth programs and reports window masks `0x17 / 0x1D / 0x17`.
Every one leaves three of the console's seven window lanes unwritten. Holes are
the **normal** case, not an edge case.

Worked example, `crater_ring`: it writes R13, R14, R15, R17 with
`out_base = 13`.

```
physical register   R13  R14  R15  R16  R17
window position       0    1    2    3    4
written?            yes  yes  yes   no  yes    -> window mask 0x17
canonical ordinal     0    1    2    -    3    -> ordinal mask 0x0F
```

A window mask can tell you a write is missing. **It cannot compact window slots
0, 1, 2, 4 into canonical result slots 0, 1, 2, 3.** Assigning one to the other
does not produce an error; it produces a plausible wrong number. The stamp
adapter reads window lanes 0-1 and the flow adapter reads lanes 3-5, so a
program writing only lane 6 would feed flow three zero velocities -- a
convincing maximum deceleration.

### `OUTPUT_MAP` is the translation, not a direct wire

One row per **ordinal**, carrying `source_kind` and `source_index`:

```
window_mask[k]   set iff there is an ordinal j with
                 source_kind[j] == VECTOR_REG and
                 source_index[j] == out_base + k

export_value[j]  the granted write to source_index[j]          (VECTOR_REG)
                 the sealed prepared scalar at source_index[j]  (PREPARED_SCALAR)

seen[j]          per ORDINAL, set by the write targeting source_index[j],
                 or seeded at point start for PREPARED_SCALAR

complete         (seen & required_mask) == required_mask  AND the drain fence
```

### The separation is a TYPE, not a comment

A comment saying "these are different" is exactly what the console already had,
and it is why R101 shipped. So the generated schema gives them **distinct type
names in both languages**:

* C++: `zfield::host_image::WindowMask` and `zfield::host_image::RequiredMask`
  are distinct classes with explicit constructors and no cross-conversion.
  Assigning one to the other **does not compile**. A `static_assert` pins that
  they have not collapsed into one type.
* SV: `zfh_window_mask_t` and `zfh_required_mask_t` are distinct packed structs
  of **deliberately different widths** (7 and 8), so a cross-assignment is a
  `WIDTH` diagnostic under `-Wall` rather than a silent truncation.

The compile-fail control is committed at
`tests/mutants/field_host_mask_type_confusion_mutant.cpp` and driven by
`python tools/field/gen_field_host_schema.py --compile-fail-control`, which
requires **both** polarities: the legal use must compile, and the confused
assignment must not.

### Three ordinals with no window position, each decided rather than falling through

1. **`PREPARED_SCALAR`** -- legitimate. It has no window position by
   construction and is seeded at point start (FH06). Its export must not wait
   for a vector write that an all-uniform program will never generate.
2. **`VECTOR_REG` whose `source_index` is outside
   `[out_base, out_base + OUT_LANES)`** -- a **load-time refusal, `BAD_IMAGE`**.
   The window cannot observe the write, so the ordinal can never be seen. This
   is the case that silently produces a wrong value today.
3. **Two ordinals aliasing one `source_index`** -- legal. Both `seen[j]` are set
   from the same granted write (directive section 7.1: *"Two output ordinals may
   reference one prepared scalar if the logical FPLAN legitimately produces that
   result"*).

### `required_mask == 0`

R111's standing hazard: **a plan writer who omits the mask silently restores the
defect and passes every gate.** So in this envelope, a strict production import
with `required_mask == 0` and a non-empty declared output list is a **refusal at
bind**. The permissive legacy meaning that R101 preserved is reachable only
through an explicitly named compatibility binding, never by omission (FH26:
legacy is a **named mode**, never the strict default).

---

## 2. Envelope rules

These are validation law, not advice. Every one of them is a refusal.

* **Validate sizes BEFORE using counts to allocate or read arrays.**
* `magic` is ASCII `ZFH2` = `5A 46 48 32`. `image_version` is 1.
* `total_bytes` is positive and 64-byte aligned. The last section body is
  zero-padded up to it.
* `body_crc32c` covers the whole image with bytes 12..15 zero.
  `canonical_full_image_crc32c` covers the **exact complete serialized
  `.zprog`**, including that file's own embedded CRC field. It is **not** a
  replacement canonical program hash and is **never the sole dedup key**.
* The section directory follows the header immediately, 16 bytes per entry,
  **sorted by kind**, then zero padding to a 64-byte boundary.
* Section bodies are 64-byte aligned, bounds checked, non-overlapping and fully
  covered by the CRC. `byte_length` equals a checked
  `element_bytes * element_count`, **widened before the multiply**.
* **All unknown section kinds refuse** in image revision 1. An optional section
  is a *known* section that may be absent -- never permission to ignore an
  unknown schema.
* Duplicate singleton sections refuse. Reserved fields and bits must be zero.
  Body padding lies outside `byte_length` and must be zero.
* `varying_input_mask` and `point_preload_mask` in `PROGRAM_META` are
  **canonical input ordinal** masks (at most fifteen meaningful bits). They are
  **not** physical RF register masks. Physical register coverage is sixty-four
  bits and belongs to `INIT_PROOF`.
* Every mask in `INIT_PROOF` is indexed by **physical register 0..63**.
  `initial_defined_mask` must equal
  `association_register_mask | point_register_mask`. Immutable registers are
  association-owned and must not overlap `vector_write_mask`.
  **No u32 mask may silently discard physical registers 32..63.**
* `expected_association_preload_count` is the mask **population count**, not the
  highest register number plus one.
* The unused address in a map row is `0xFFFF` (`ZFH_ADDR_UNUSED`), **never an
  implicitly valid register or slot zero**.
* `UNIFORM_ONLY` requires a checked logical plan with no varying instructions,
  all output sources valid prepared scalars, and **zero** physical uops. **An
  empty arbitrary physical program is not this optimisation.** The other forms
  include their actual terminating `END` within `physical_uop_count`.
* `PRELOAD` duplicates with conflicting values refuse. Same-value duplicates may
  be canonicalised by the packer but must not weaken the installed completeness
  check.

### Required installation checks (directive 11.5)

Check cross-record **agreement**, not just each record separately:

* header profile and identity against the decoded program;
* association against its pinned program, across handle, epoch, hash and
  versions;
* maps against counts and masks;
* the exact required table set against the table set actually received;
* preload destinations against the physical source/use proof;
* output scalar indices against the scalars actually prepared or written;
* code length against `END` and the physical instruction store capacity;
* the desired execution class against the operations the fabric actually
  supports.

A CRC does not mathematically prove a program. Software verification
establishes the physical initialization proof; hardware ensures that it
installed the certified image and did not skip a required preload.

---

## 3. Instruments

`--check` runs seven checks. Each is listed with **what its two operands are
driven by**, because CLAUDE.md's metadata-swap chapter says that is the first
question to ask of any checker: if one source drives both sides, the comparison
is structurally blind to every fault that source participates in.

| # | Check | Operand A | Operand B |
|---|---|---|---|
| A | schema self-consistency | the schema | (an internal invariant, not a comparison) |
| B | parser self-tests | the historical bad rows | the parser |
| C | freshness | regenerated text | the three committed artifacts |
| D | C++/SV offset parity | the committed `.hpp`, parsed | the committed `.sv`, parsed |
| E | FH21 opcode-shape parity | `zhao_field_ops_pkg.sv` | `zfield_optable.hpp` |
| F | profile arity | the schema | `spec/form/field-ir.md` 7.1 |
| G | profile arity, fourth copy | the schema | `zprog_output_coverage.py` |

**Check D is deliberately not compared against the schema.** Comparing each
generated file to the schema would move both operands together whenever the
schema changes, which catches staleness but is blind to a hand-edit. Parsing the
two committed files and comparing them *to each other* fires on a hand-edit to
either side. That is the negative control this schema owes, and it has been seen
to fire.

**Check B fires the arity parser on the defect it exists for.** The historical
Formation row said `(11)` against twelve listed fields and the historical Warp
row said `(14)` against fifteen. Both must be **rejected** by the parser and the
corrected rows **accepted**, asserted at import. A parser that only ever agrees
is a parser nobody has tested.

**Check E is bidirectional, per ruling R110.** A check that asks only "is
everything declared present?" and never "is everything present declared?"
reports a clean ledger for a tree whose ledger is half missing -- and the defect
makes the report *shorter*, which is the flattering direction. So E checks both
that every opcode the SV table declares has the C++ shape it claims, **and**
that every canonical opcode routed to a service appears in the SV table. The
second direction currently names two **outstanding routes**, RCP (`0x17`) and
RING (`0x21`), which is the directive's own finding at section 2.6 and
`zhao_field_ops_pkg.sv:68` arrived at independently. They are reported, not
skipped, and a *new* absentee fails the check.

**Every parser here strips comments before matching and resolves against a
declaration, never a substring of raw text** -- ruling R105, where a check that
matched raw bytes would accept a name appearing only in a comment.

---

<!-- BEGIN GENERATED: record-layout (gen_field_host_schema.py) -->

*Generated by `tools/field/gen_field_host_schema.py`. Do not edit between the markers; edit the schema and regenerate.*

`schema-sha256: 162fb7f8e391715c3b2078d9ca5c3d0b9a79bc06633ad8e9c7711b03d4b65d1c`

### ZfhHeader -- 64 bytes

Common 64-byte image header (directive 11.1). Present at offset 0 of every ZFH2 image, PROGRAM and ASSOCIATION alike.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 4 | bytes[4] | `magic` | ASCII 'ZFH2' = 5A 46 48 32 |
| 4 | 2 | u16 | `image_version` | = 1 in this revision |
| 6 | 1 | u8 | `object_kind` | 0 PROGRAM, 1 ASSOCIATION |
| 7 | 1 | u8 | `flags` | zero in this revision |
| 8 | 4 | u32 | `total_bytes` | positive, 64-byte aligned |
| 12 | 4 | u32 | `body_crc32c` | whole image with bytes 12..15 zero |
| 16 | 2 | u16 | `host_protocol_version` | = 2 |
| 18 | 2 | u16 | `reserved_18` | reserved, must be zero |
| 20 | 4 | u32 | `canonical_program_hash` |  |
| 24 | 4 | u32 | `canonical_program_handle32` |  |
| 28 | 4 | u32 | `resource_epoch` |  |
| 32 | 4 | u32 | `logical_fplan_abi_version` |  |
| 36 | 4 | u32 | `execution_fabric_version` |  |
| 40 | 4 | u32 | `source_id32` |  |
| 44 | 2 | u16 | `section_count` |  |
| 46 | 2 | u16 | `header_bytes` | = 64 |
| 48 | 4 | u32 | `parent_binding_handle` | zero for an unbound PROGRAM install |
| 52 | 4 | u32 | `canonical_full_image_crc32c` | all serialized .zprog bytes, including that file's own embedded CRC field; never the sole dedup key |
| 56 | 4 | u32 | `object_serial` | software identity; hardware also assigns a generation |
| 60 | 4 | u32 | `reserved_60` | reserved, must be zero |

### ZfhSectionEntry -- 16 bytes

One section-directory entry (directive 11.2). section_count of these follow the header immediately, sorted by kind, then zero padding to a 64-byte boundary.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 2 | u16 | `kind` | a ZFH_SECTION_* value |
| 2 | 2 | u16 | `element_bytes` |  |
| 4 | 4 | u32 | `element_count` |  |
| 8 | 4 | u32 | `offset` | 64-byte aligned body offset |
| 12 | 4 | u32 | `byte_length` | checked element_bytes * element_count, widened before multiply |

### ZfhProgramMeta -- 64 bytes

PROGRAM_META, exactly one 64-byte record (directive 11.3). The final reserved words are not capacity for undocumented policy.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 1 | u8 | `profile` | 0 earth, 1 warp, 2 flow, 3 formation, 4 stamp |
| 1 | 1 | u8 | `binding_signature` |  |
| 2 | 1 | u8 | `execution_form` | 0 CANONICAL, 1 PREPARED_REGISTER, 2 UNIFORM_ONLY |
| 3 | 1 | u8 | `flags` |  |
| 4 | 1 | u8 | `input_count` |  |
| 5 | 1 | u8 | `output_count` | 1..7 at a production binding |
| 6 | 1 | u8 | `required_mask` | ORDINAL-INDEXED. Read the two-mask chapter before wiring this. Zero is a load-time refusal for a strict import. |
| 7 | 1 | u8 | `table_count` | 0..4 |
| 8 | 2 | u16 | `canonical_instruction_count` |  |
| 10 | 2 | u16 | `physical_uop_count` | includes the terminating END for the non-uniform forms |
| 12 | 2 | u16 | `physical_register_count` |  |
| 14 | 2 | u16 | `prepared_scalar_count` |  |
| 16 | 4 | u32 | `varying_input_mask` | CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits; NOT a physical RF register mask |
| 20 | 4 | u32 | `point_preload_mask` | CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits |
| 24 | 4 | u32 | `code_image_crc` |  |
| 28 | 4 | u32 | `maps_crc` |  |
| 32 | 4 | u32 | `logical_plan_identity` |  |
| 36 | 28 | u32[7] | `reserved` | reserved, must all be zero |

### ZfhInputMapRow -- 8 bytes

INPUT_MAP, one 8-byte row per canonical input ordinal (directive 11.3). Validate as a complete ordinal set.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 1 | u8 | `ordinal` |  |
| 1 | 1 | u8 | `lane_type` | code from the canonical Field I/O-map registry |
| 2 | 1 | u8 | `source_class` | 0 VARYING_INPUT, 1 UNIFORM_INPUT, 2 UNUSED_PROVEN |
| 3 | 1 | u8 | `flags` |  |
| 4 | 2 | u16 | `physical_register` | ZFH_ADDR_UNUSED when not applicable, never an implicitly valid register zero |
| 6 | 2 | u16 | `prepared_slot` | ZFH_ADDR_UNUSED when unused |

### ZfhOutputMapRow -- 8 bytes

OUTPUT_MAP, one 8-byte row per canonical output ORDINAL (directive 11.3). THIS RECORD IS THE ORDINAL-TO-WINDOW TRANSLATION. Reserved or unknown source_kind values refuse.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 1 | u8 | `ordinal` | canonical output ordinal j |
| 1 | 1 | u8 | `lane_type` |  |
| 2 | 1 | u8 | `source_kind` | 0 VECTOR_REG, 1 PREPARED_SCALAR |
| 3 | 1 | u8 | `flags` | bit0 REQUIRED; no undocumented meanings |
| 4 | 2 | u16 | `source_index` | physical register for VECTOR_REG, prepared-scalar index for PREPARED_SCALAR |
| 6 | 2 | u16 | `reserved` | reserved, must be zero |

### ZfhInitProof -- 64 bytes

INIT_PROOF, exactly one 64-byte record (directive 11.3). EVERY MASK HERE IS INDEXED BY PHYSICAL REGISTER 0..63 -- no u32 mask may silently discard registers 32..63.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 2 | u16 | `proof_version` | = 1 |
| 2 | 2 | u16 | `record_bytes` | = 64 |
| 4 | 4 | u32 | `flags` | bit0 canonical validation, bit1 prepared physical validation; exactly the one matching execution_form is set |
| 8 | 8 | u64 | `association_register_mask` |  |
| 16 | 8 | u64 | `point_register_mask` |  |
| 24 | 8 | u64 | `immutable_register_mask` | association-owned; must not overlap vector_write_mask |
| 32 | 8 | u64 | `initial_defined_mask` | must equal association_register_mask | point_register_mask |
| 40 | 8 | u64 | `vector_write_mask` |  |
| 48 | 4 | u32 | `code_image_crc` |  |
| 52 | 4 | u32 | `maps_crc` |  |
| 56 | 2 | u16 | `expected_association_preload_count` | the mask POPULATION COUNT, not the highest register plus one |
| 58 | 2 | u16 | `reserved_58` | reserved, must be zero |
| 60 | 4 | u32 | `reserved_60` | reserved, must be zero |

### ZfhAssociationMeta -- 64 bytes

ASSOCIATION_META, exactly one 64-byte record (directive 11.4). The varying and uniform input masks are disjoint and together cover every required used input; unused inputs are declared, not omitted.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 4 | u32 | `association_serial` |  |
| 4 | 4 | u32 | `frame_id` |  |
| 8 | 1 | u8 | `client_id` |  |
| 9 | 1 | u8 | `profile` |  |
| 10 | 1 | u8 | `execution_class` |  |
| 11 | 1 | u8 | `flags` |  |
| 12 | 4 | u32 | `expected_points` | useful application points, not physical padding lanes |
| 16 | 4 | u32 | `varying_input_mask` | canonical input ordinals |
| 20 | 4 | u32 | `uniform_input_mask` | canonical input ordinals |
| 24 | 4 | u32 | `preparation_serial` |  |
| 28 | 4 | u32 | `max_group_quantum` |  |
| 32 | 4 | u32 | `source_id` |  |
| 36 | 4 | u32 | `numeric_uniform_status` | only generated defined bits are used |
| 40 | 8 | u64 | `client_binding_cookie` |  |
| 48 | 16 | u32[4] | `reserved` | reserved, must all be zero |

### ZfhPreloadRow -- 8 bytes

PRELOAD, one 8-byte row (directive 11.4). Duplicates with conflicting values refuse; same-value duplicates may be canonicalised by the packer but must not weaken the installed completeness check.

| Offset | Bytes | Type | Field | Note |
|---|---|---|---|---|
| 0 | 2 | u16 | `physical_register` | 0..63 |
| 2 | 2 | u16 | `flags` |  |
| 4 | 4 | s32 | `value` |  |

### Section kinds

| Kind | Value | Note |
|---|---|---|
| `CANONICAL_IMAGE` | `0x0001` | the validated .zprog, exact bytes |
| `PROGRAM_META` | `0x0002` | singleton |
| `PHYSICAL_UOPS` | `0x0003` | 8-byte native words |
| `INPUT_MAP` | `0x0004` | one row per canonical input ordinal |
| `OUTPUT_MAP` | `0x0005` | one row per canonical output ordinal |
| `INIT_PROOF` | `0x0006` | singleton |
| `DEMAND` | `0x0007` | optional; per-resource counts for the selected form |
| `TABLE0` | `0x0010` | canonical table body bytes, t=0 |
| `TABLE1` | `0x0011` | canonical table body bytes, t=1 |
| `TABLE2` | `0x0012` | canonical table body bytes, t=2 |
| `TABLE3` | `0x0013` | canonical table body bytes, t=3 |
| `ASSOCIATION_META` | `0x0020` | singleton |
| `CANONICAL_INPUTS` | `0x0021` | 15 raw words, 60 bytes + 4 zero pad |
| `PREPARED_SCALARS` | `0x0022` | exact values from zfield::prepare |
| `PRELOAD` | `0x0023` | one row per required physical register |
| `LEDGER` | `0x0024` | optional; exact uniform per-cause ledger |

### Profile arity (`spec/form/field-ir.md` 7.1)

| Profile | id | Inputs | Outputs |
|---|---|---|---|
| earth | 0 | 12 | 4 |
| warp | 1 | 15 | 6 |
| flow | 2 | 13 | 7 |
| formation | 3 | 12 | 6 |
| stamp | 4 | 8 | 3 |

<!-- END GENERATED: record-layout -->
