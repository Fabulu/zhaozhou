# Zhaozhou Capture Format — Command ABI, Frame Packet, .zcap Container

**Status:** Phase 1 ratified (wave 1, plan RUN-20260814-1912 W4 / P5 recon
RUN-20260814-1852-wave1-abi-capture); **ABI v2 amendments ratified in wave 2**
(plan W2.1/D5/D6/D8, run RUN-20260814-2154): `video_mode` enum, `PadFrame`
struct, implemented debug commands (0xF002/0xF004), `angle16` wire type,
`DrawSky 0x0310` reservation (spec/sky_and_beams.md 4). **Wave-3 amendments**
(plan W3.1/D7/D11, run RUN-20260815-0544): Phase-3 command promotions
(§1.2 note), source-ID kinds 8-11 (§5), binary `sourceids.zmap` (§7),
`.zpak` cartridge container (spec/cartridge.md, which reuses §4 verbatim).
**ABI v3 amendment (2026-08-17, lighting & pose consolidation wave, run
RUN-20260816-0046):** `SetEnvironment 0x0311` reserved (spec/sky_and_beams.md
§4a, the sky extension range's first allocation — that file's v1.2), `rgb565`
struct + `fog_mode` enum, and two state chunks — `CELESTIAL_STATE 0x000B`
(spec/stars_and_flares.md §8, landing the chunk that spec declared) and
`ENVIRONMENT_STATE 0x000C` (§4.2). The v2→v3 bump is the new-opcode wire
change the frozen rule names (a v2 decoder reports `ZH_ABI_UNKNOWN_OPCODE`
on 0x0311); committed wave-2 captures pin `abi_version 2` and remain valid
historical evidence — a v3 replay refuses them by design, which is the bump
buying what it costs. The same wave's `QFMT_VERSION` 1→2 (qformats §7.6,
amendment C1) is a numeric-law constant change only and did NOT bump the ABI
version (no opcode, field set, or size moved; the frame wire is unchanged).
Single source of
truth for everything that crosses a language or a machine boundary.
`spec/commands.zidl` is the machine-readable ABI definition; this document is
its law and the law of the two containers built on it.

Fixed-point types (`fx16` = Q16.16 in a 4-byte int32 container) are defined by
`spec/qformats.md` (W3) — that file is the authority; this one only references
the type names.

**Byte order:** little-endian everywhere, including stored CRC words. Stored
CRC words are plain u32 values in this byte order (RFC 3720 12.1 devotes a
bullet list to CRC byte mapping because it is the number-one interop bug
class; we state it once, here, as a rule).

---

## 1. The .zidl grammar

`spec/commands.zidl` is parsed by `tools/abi-gen` (hand-written recursive
descent, zero external parser dependencies). The grammar (EBNF, P5 1.2 with
two ratified extensions marked **[w1]**):

```ebnf
(* Lexical: C-style comments // and /* */ ; identifiers [A-Za-z_][A-Za-z0-9_]* ;
   integers decimal or 0x hex. *)

zidl         = abi_decl , { statement } , eof ;

abi_decl     = "abi" , ident , "{" , { abi_attr } , "}" ;
abi_attr     = "version" , int
             | "endian" , ( "little" | "big" )          (* Phase 1: little only *)
             | "command_alignment" , int                (* Phase 1: 16 *)
             | "opcode_width" , ( "u8" | "u16" ) ;      (* Phase 1: u16 *)

statement    = const_decl | enum_decl | struct_decl | command_decl ;

const_decl   = "const" , prim_type , ident , "=" , int , ";" ;

enum_decl    = "enum" , ident , [ ":" , prim_type ] , "{" , { enum_entry } , "}" ;
enum_entry   = ident , "=" , int , ";" ;

struct_decl  = "struct" , ident , "{" , { field } , "}" ;

command_decl = "command" , ident , hex_int ,
               [ "implemented" | "reserved" ] ,          (* [w1] default: error *)
               "{" , { field } , "}" ;

field        = field_type , ident , [ array_suffix ] , ";" ;
field_type   = prim_type | handle_type | enum_ref | struct_ref ;

prim_type    = "u8" | "u16" | "u32" | "u64"
             | "i8" | "i16" | "i32" | "i64"
             | "fx16" | "fx32" | "fx64"                  (* Q formats: qformats.md *)
             | "angle16"                                 (* [v2] U 0.0.16 turns, u16 *)
             | "pad" ;                                   (* one byte; pad[12] etc. *)

handle_type  = "handle32" , [ "[" , ident , "]" ] ;       (* u32 {index:24, gen:8} *)

enum_ref     = enum_decl_ident ;
struct_ref   = struct_decl_ident ;

array_suffix = "[" , int , "]" ;                          (* fixed length only *)
```

Ratified wave-1 extensions:

1. **Command status keyword.** `implemented` commands are executed by the
   decoder/shell; `reserved` commands have a frozen wire layout and validate
   structurally, but carry no execution semantics yet — an executor asked to
   run one reports `ZH_ABI_UNIMPLEMENTED_COMMAND`. Omitting the keyword is a
   generator error (status is part of the ABI contract, not an optional
   comment). Since v2, debug-umbrella opcodes (0xF000-0xF0FF) MAY be
   `implemented` — the console shell executes them (debug-blit DMA, rumble),
   they stay never-game-facing, and the header flags-bit0 requirement is
   unchanged.
2. **`bits` containers are legal grammar but unused in v1.** The generator
   implements them (LSB-first members, overflow rejected); no v1 command
   declares one. Bitfield-having commands activate the `ZH_ABI_BAD_VALUE`
   checks when they appear.

### 1.1 Layout rules (enforced, never inferred)

1. Little-endian; field offsets accumulate in declaration order.
2. A field of size S starts at an offset divisible by `min(S, 4)`. Padding is
   **explicit** (`pad[n]` fields) — the generator hard-errors on implicit
   padding, so the .zidl is the complete truth.
3. Structs and arrays multiply the element layout; element alignment is
   `min(element_size, 4)` capped at 4.
4. A command's total record size (16-byte command header + payload, 2) must be
   a multiple of `command_alignment` (16); trailing pad is declared, not
   hidden.
5. `pad` fields and `reserved` handle/kind words must be zero on the wire;
   nonzero is `ZH_ABI_RESERVED_FIELD`.
6. `fx16` is 4 bytes (Q16.16 in int32); `fx32` is 8 bytes (Q32.32 in int64).
   The *name* counts fractional bits, not container bytes. `angle16`
   (**[v2]**) is 2 bytes — an unsigned 16-bit *turns* phase (qformats.md 2);
   u16 add/sub is exact mod one turn.
7. (**[v2]**) A struct's total size must be a multiple of its alignment cap
   `min(max member size, 4)` — the generator hard-errors otherwise — so a
   struct used as an array element keeps every member of every element
   naturally aligned (rule 3). Tail padding is explicit, never hidden.
8. (**[v2]**) Enum-typed fields (`enum video_mode : u8` etc.) occupy their
   backing primitive's width; a value outside the declared member set is
   `ZH_ABI_BAD_VALUE` (3.2 step 7 — active since v2).

### 1.2 Opcode ranges

| Range | Meaning | v1 |
|---|---|---|
| `0x0000-0x00FF` | frame control, views, presentation | Nop 0x0000, BeginFrame 0x0001, EndFrame 0x0002, SetView 0x0010, SetPresentationContract 0x0020 |
| `0x0200-0x02FF` | terrain / surface | TerrainField 0x0200, SurfaceStamp 0x0210 (**implemented [w3]**, D7 — software console executes them) |
| `0x0300-0x03FF` | forms / populations / procedural | DrawForm 0x0300, DrawPopulation 0x0301, DrawProcedural 0x0302 (**implemented [w3]**, D7); DrawSky 0x0310 reserved [v2, spec/sky_and_beams.md 4; wave 8]; SetEnvironment 0x0311 reserved [v3, spec/sky_and_beams.md §4a — the light/environment state record]; `0x0312-0x031F` reserved for sky extensions (never allocate without a spec/sky_and_beams.md version bump) |
| `0x0400-0x04FF` | audio | EmitAudioEvent 0x0400 (**implemented [w3]**, D7 — wave-2 mixer tone) |
| `0xF000-0xF0FF` | bootstrap/debug umbrella | DebugBootstrap 0xF001 (reserved), DebugFrameBlit 0xF002 (implemented [v2]), DebugRumble 0xF004 (implemented [v2]) — never game-facing |

**[w3] Promotion law (D7):** the six Phase-3 promotions are execution-
semantics changes only — record layouts, opcodes and sizes are untouched
(except the pad-byte reinterpretations noted in 1.3, which preserve every
byte offset and total size). The `.zidl` `abi version` therefore stays **2**:
its frozen rule (below) bumps only for wire changes, and every change in
wave 3 is additive or same-bytes reinterpretation of never-executed reserved
payload.

Opcodes are frozen once shipped: opcode, field set and sizes never change;
additive change = new opcode, old kept as deprecated alias. A frame containing
a `0xF000-0xF0FF` opcode MUST set frame header flags bit0
`contains_debug_commands`, else `ZH_ABI_DEBUG_FLAG_REQUIRED` (plan 1.E).

### 1.3 v1 record sizes (generator-computed; goldens are the evidence)

| Command | Opcode | Record bytes | Status |
|---|---|---|---|
| Nop | 0x0000 | 16 | implemented |
| BeginFrame | 0x0001 | 32 | implemented |
| EndFrame | 0x0002 | 32 | implemented |
| SetView | 0x0010 | 96 | implemented |
| SetPresentationContract | 0x0020 | 48 | implemented |
| TerrainField | 0x0200 | 112 | **implemented [w3]** |
| SurfaceStamp | 0x0210 | 64 | **implemented [w3]** |
| DrawForm | 0x0300 | 32 | **implemented [w3]** |
| DrawPopulation | 0x0301 | 32 | **implemented [w3]** |
| DrawProcedural | 0x0302 | 64 | **implemented [w3]** |
| EmitAudioEvent | 0x0400 | 32 | **implemented [w3]** |
| DrawSky | 0x0310 | 176 | reserved |
| SetEnvironment | 0x0311 | 48 | reserved [v3] |
| DebugBootstrap | 0xF001 | 64 | reserved |
| DebugFrameBlit | 0xF002 | 48 | implemented |
| DebugRumble | 0xF004 | 32 | implemented |

**[w3] Same-bytes reinterpretations** (the v2 `u8 mode` → `video_mode`
precedent applied to never-executed reserved payload; every byte offset and
the record total are unchanged, old all-zero-pad frames remain valid):
`DrawProcedural` payload byte 36 (was `pad[12]` byte 0) is now
`forge_kind` (u8 enum, 0 = `heightfield_patch` — zero was already mandatory
for pads, so ABI-v2-era captures stay valid); `SurfaceStamp` payload bytes
36-43 (was `pad[12]` bytes 0-7) are now `radius`/`ring_width` (fx16 × 2,
circle/ring stamp geometry per D7). Both are validated range-wise as enum /
fx16 fields from wave 3 on; the details are law in `spec/commands.zidl`.

Deviations from the P5 recon table (record sizes there were estimates; the
.zidl layout math above is normative and the differences are all consequences
of ratified decisions — 4-byte `fx16`, 24-byte `transform2fx` = 6 × fx16):
SetPresentationContract pads `pad[8]` (recon said `pad[4]`, which cannot reach
a 16-multiple); TerrainField is 112 B, not 96 (64 parameter bytes + handle +
rect + two ticks cannot fit 80 payload bytes under any padding); SurfaceStamp
and DrawProcedural are 64 B (24-byte transform2fx instead of the recon's
16-byte guess); SetView needs no pads (80 payload bytes exactly). Byte maps:
`spec/generated/abi.md` (regenerated from the .zidl).

---

## 2. CRC-32C

Parameter set **CRC-32/ISCSI** (RevEng catalogue): width 32, poly
`0x11EDC641` (reflected `0x82F63B78`), init `0xFFFFFFFF`, refin/refout true,
xorout `0xFFFFFFFF`. Chosen over IEEE CRC-32 for Hamming distance: HD 6 up to
~654 B (a frame packet), HD 4 up to 2^31 bits; hardware-accelerated on both
host ISAs (x86 SSE4.2, ARMv8 FEAT_CRC32 `CRC32C*`); identical cost on FPGA.

Reference form (identical in C++/TS; SV uses the per-byte step of 2.2):

```c
uint32_t crc32c(uint32_t crc, const void* buf, size_t len) {  // crc = running value or 0
    crc = ~crc;                        // absorb init on incremental calls
    while (len--)
        crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;                       // final xorout
}
```

The 256-entry table is emitted **as a literal constant** into all three
generated modules (deterministic, timestamp-free output).

### 2.1 Test vectors (normative; see tests/unit/test_crc.cpp)

| Input | CRC-32C | Stored (LE) |
|---|---|---|
| empty (0 B) | `0x00000000` | `00 00 00 00` |
| `"123456789"` | `0xE3069283` | `83 92 06 E3` |
| 32 × `0x00` | `0x8A9136AA` | `AA 36 91 8A` |
| 32 × `0xFF` | `0x62A8AB43` | `43 AB A8 62` |
| 32 B `0x00..0x1F` | `0x46DD794E` | `4E 79 DD 46` |
| 32 B `0x1F..0x00` | `0x113FDB5C` | `5C DB 3F 11` |
| **check constant**: finalized CRC over (message ‖ stored CRC LE) — same value for EVERY message | `0x48674BC7` | — |
| register residue (init `0xFFFFFFFF` seeded, no xorout) after the same bytes | `0xB798B438` (RevEng catalogue CRC-32/ISCSI residue) | — |

Two recon claims were flagged "verify in-suite" and both are now settled by
the suite (`tests/unit/test_crc.cpp` + abi-gen tests, C++/TS/SV agree):

- **Empty input `0x00000000`** — the derivation init ⊕ xorout = 0 is
  **correct**; asserted against the real implementation.
- **Residue `0x1C2D19ED`** (P5 2.4) — **wrong for this parameterization**:
  not reproducible in the finalized form, the raw register form, or the
  init-seeded form (verified empirically 2026-08-14). The correct
  self-test contract is the check constant above: for every message,
  `crc32c(0, message ‖ LE(crc32c(0, message))) == 0x48674BC7`, i.e. the
  init-seeded register (no xorout) ends at the catalogue residue
  `0xB798B438`. This is the cross-language RTL self-test; the C++/TS/SV
  implementations assert the same constant.

### 2.2 SystemVerilog step (synthesizable)

```systemverilog
function automatic logic [31:0] zhao_crc32c_step(input logic [31:0] c,
                                                 input logic [7:0]  d);
    logic [31:0] crc = c ^ {24'b0, d};
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);
    return crc;
endfunction
// frame: seed 32'hFFFFFFFF, one step per byte, result = ~crc
```

The per-byte invariant `crc' = table[(crc^byte)&0xFF] ^ (crc>>8)` is the
cross-language contract: the emitted table (C++/TS) and the emitted step
function (SV) are cross-validated by goldens and by the Verilated probe.

### 2.3 Where CRCs live

- Frame packet: `header_crc32c` (over header bytes [0,32)) and trailing
  `payload_crc32c` (over the command stream) — 3.
- .zcap: `header_crc32c` (over file bytes [0,8)) and one `section_crc32c` per
  section body — 5. No whole-file CRC (per-section CRCs permit partial
  recovery of truncated captures; pcapng carries none at all).
- Program identity (W5): CRC-32C over code+tables per plan 1.B-7.

---

## 3. Frame packet (one frame slot)

```
Offset  Size  Field                Notes
0       4     magic                'Z','P','K','1' on the wire (u32 LE 0x314B505A)
4       2     abi_version          u16; must equal the generated ABI version
6       2     flags                u16; bit0 = contains_debug_commands; bits 1-15 must be 0
8       4     frame_id             u32
12      4     sequence             u32 (monotonic packet sequence)
16      4     resource_epoch       u32 (charter 7.4 seal component)
20      4     deadline_cycles      u32
24      4     command_count        u32 (records in the stream)
28      4     command_bytes        u32 (stream length; multiple of 16)
32      4     header_crc32c        CRC-32C over bytes [0,32)
36      N     command stream       N = command_bytes; sequence of records (3.1)
36+N    4     payload_crc32c       CRC-32C over the command stream
```

Total = `40 + N`, `N ≡ 0 (mod 16)`, total ≤ `FRAME_SLOT_BYTES` (1 MiB,
provisional, plan 1.E). **Dual CRC, frame level only** (plan 1.E: no
per-command CRC): the decoder validates the 32-byte header before trusting
`command_bytes`/`command_count`; a corrupted length is caught by
`header_crc32c` rather than causing a wild read. Command integrity = payload
CRC + per-record length/opcode checks.

Seal protocol (charter 7.4): producer writes header + stream + payload CRC +
header CRC last, then commits the slot ARM_WRITING → READY; the slot is never
touched again; the FPGA never exposes a partially consumed slot.

### 3.1 Command record framing

```
Offset  Size  Field          Notes
0       2     opcode         u16; must exist in the ABI table
2       2     record_bytes   u16; total record size incl. header; multiple of 16; >= 16
4       4     source_id      u32; scheme per 6; 0x00000000 = ZH_SOURCE_NONE (bootstrap)
8       4     flags          u32; no v1 command defines record-header flag bits -> must be 0
12      4     reserved0      u32; MUST be 0
16      ..    payload        (record_bytes - 16) bytes, layout per .zidl / spec/generated/abi.md
```

`handle32` wire format: `{ index: 24 bits, generation: 8 bits }`. Consumers
compare `generation` against the current epoch's generation and reject stale
handles with `ZH_ABI_STALE_HANDLE`. An authored `page_id` remains a full u32
identity; it is never converted to a handle by masking to 24 bits. Build
preflight deterministically maps `(resource role, full u32 page/resource ID)`
to a collision-free handle index, and generated metadata carries that mapping.
Thus IDs 1 and 16777217 are distinct even though their low 24 bits match, and
the same numeric ID in different roles (form page, procedural patch, stamp
brush, population, sound) is also distinct. Transient handles allocate only
after all authored mapping indices are reserved. **v2 scope:** enum range checks are LIVE —
`SetPresentationContract.mode` and `DebugFrameBlit.mode` carry `video_mode`
(u8, members 0-2); an out-of-range value fails with `ZH_ABI_BAD_VALUE`
(corpus case `enum_out_of_range`). The stale-handle check remains generated
but unexercised (no live command carries a handle yet; DrawSky will activate
it in wave 8).

### 3.2 Validation order (fail-safe; normative)

Every check runs **before** any payload field is consumed. On any error the
frame aborts: no partial consumption, no writes outside assigned memory
(charter 20.4 formal property). Error codes are the generated
`zhao_abi_error` enum, shared verbatim across C++/TS/SV.

| # | Check | Error |
|---|---|---|
| 1 | magic (checked whenever at least 4 bytes exist; `n < 4` alone is BAD_LENGTH) | `ZH_ABI_BAD_MAGIC` |
| 1 | packet length at least the 36-byte header | `ZH_ABI_BAD_LENGTH` |
| 1 | abi_version | `ZH_ABI_BAD_ABI_VERSION` |
| 1 | frame flags bits 1-15 zero | `ZH_ABI_RESERVED_FLAG` |
| 2 | `40 + command_bytes <= FRAME_SLOT_BYTES`; `command_bytes % 16 == 0`; `command_count * 16 <= command_bytes`; packet length exactly `40 + command_bytes` | `ZH_ABI_BAD_LENGTH` |
| 3 | header_crc32c (bytes [0,32)) | `ZH_ABI_BAD_HEADER_CRC` |
| 4 | payload_crc32c (bytes [36, 36+N)) | `ZH_ABI_BAD_PAYLOAD_CRC` |
| 5 | per record, in stream order: `record_bytes % 16 == 0 && >= 16`; running sum + record_bytes ≤ command_bytes; opcode known; `record_bytes == LayoutIR[opcode].size` | `ZH_ABI_BAD_LENGTH` / `ZH_ABI_UNKNOWN_OPCODE` |
| 6 | record header `reserved0 == 0`; record header `flags == 0` (no defined bits in v1); payload pad bytes zero | `ZH_ABI_RESERVED_FIELD` / `ZH_ABI_RESERVED_FLAG` |
| 7 | enum ranges / bitfield widths | `ZH_ABI_BAD_VALUE` (v2: `video_mode` fields) |
| 8 | handle generations vs resource_epoch | `ZH_ABI_STALE_HANDLE` (no v1 field) |
| 9 | records sum exactly to command_bytes; records walked == command_count | `ZH_ABI_TRUNCATED` / `ZH_ABI_COUNT_MISMATCH` |
| 10 | any opcode in `0xF000-0xF0FF` requires frame flags bit0 | `ZH_ABI_DEBUG_FLAG_REQUIRED` |

Checks 1-3 (header-level) abort after consuming exactly 36 bytes; otherwise
the whole `40 + N` packet is consumed before the verdict. `bytes_consumed` in
every implementation (ZRef, stub RTL, tools) therefore means: 36 on a
header-level abort, else `40 + N`.

### 3.3 Execution semantics (Phase 1)

The decoder shell executes `implemented` commands as no-ops with counters
(charter Phase-1 build list; real semantics land with CMD.*/later blocks):
`commands_total`, `begin_frames`, `end_frames`, `nops`. A frame using a
`reserved` command is a **valid packet** (structure is frozen and validated)
whose execution reports `ZH_ABI_UNIMPLEMENTED_COMMAND` — packet validity and
execution capability are separate axes.

---

## 4. .zcap container

### 4.1 File layout

```
Offset  Size  Field
0       4     magic 'Z','C','A','P'  (u32 LE 0x5041435A)
4       2     format_version = 1
6       2     flags               bit0 = little_endian (must be 1 in v1); bits 1-15 reserved 0
8       4     header_crc32c       CRC-32C over bytes [0,8)
12      4     section_count       u32
16      4     section_table_offset = 32
20      4     section_entry_size = 32
24      8     total_file_length   u64   (plan 1.E: u64 lengths day one)
32      ..    section table: section_count x 32-B entries

Section-table entry:
+0  u16  section_type
+2  u16  section_version        (independent per section type)
+4  u16  flags                  bit0 = crc_present; bits 1-15 reserved 0
+6  u16  reserved               0
+8  u64  body_offset            (from file start)
+16 u64  body_length
+24 u32  section_crc32c         CRC-32C over body; 0 if crc_present = 0
+28 u32  reserved               0
```

### 4.2 Section types (Phase 1)

| Type | Name | Contents |
|---|---|---|
| 0x0001 | ABI_INFO | `u32 abi_version; u32 zcap_schema_version; u8 generator_name[16]; u8 generator_sha256[32]; u8 zidl_sha256[32]` — must be first |
| 0x0002 | FRAME_PACKET | raw sealed frame packet bytes (3) |
| 0x0003 | RESOURCE_PAGES | `u32 count` + count × `{u8 kind; u8 rsv[3]; u32 page_id; u64 byte_length; u8 sha256[32]; u8 ref[64]}` |
| 0x0004 | CONTROLLER_SNAPSHOT | `u32 count` + count × `PadFrame` — the GENERATED struct from spec/commands.zidl (v2): `{u8 pad_index; u8 flags; u16 sequence; u32 buttons; i16 lx, ly, rx, ry; u32 rsv}` = 20 B, array stride 20. Semantics: spec/input_rules.md. (The wave-1 sketch ended `u16 rsv` = 18 B; the grammar's rule-3 stride law requires one explicit 4-B reserved word — the generator now enforces it and this doc follows the .zidl.) |
| 0x0005 | FRAMEBUFFER_EXPECTED | `{u8 mode; u8 view_count; u16 flags; u16 width; u16 height; u16 rsv; u32 expected_crc32c}` — `mode` is the `video_mode` enum (v2: same u8 byte, 0=Z60, 1=Storm, 2=Duo); `expected_crc32c` covers the DISPLAYED stream after the repeat decision (spec/video_rules.md) |
| 0x0006 | TILE_CRC | `u32 count` + count × `{u32 tile_index; u32 crc32c}` |
| 0x0007 | DEPTH_STENCIL_CRC | optional; same shape as TILE_CRC |
| 0x0008 | COUNTERS | `u32 count` + count × `{u16 counter_id; u16 rsv; u64 expected_value}` |
| 0x0009 | SOURCE_MAP | 6 |
| 0x000A | TRACE | first-divergence record (charter 20.6): `{u32 tile; u32 primitive; u32 pixel; u8 stage; u8 rsv[3]; u32 expected_fx; u32 actual_fx; u32 source_id; u32 command_seq}` |
| 0x000B | CELESTIAL_STATE | **[v3]** fixed **236 B** little-endian, the chunk `spec/stars_and_flares.md` §8 declares (layout below, semantics and change control stay that file's) |
| 0x000C | ENVIRONMENT_STATE | **[v3]** fixed **20 B** little-endian, the light/environment state mirror of `SetEnvironment` (layout below; law: `spec/sky_and_beams.md` §4a) |
| 0x8000-0xFFFF | tool namespace | tools may add private sections; readers MUST skip |

**[v3] CELESTIAL_STATE body (236 B — the reference serializer is the
evidence, `reference/src/zsky/star_gamut.cpp`, round-trip asserted in
`tests/render/render_star.cpp`):**

```
+0    56  ramp slew state, near star 0: 12 x s16 cur, 12 x s16 tgt,
          u8 init, 7 x u8 reserved (0)
+56   56  ramp slew state, near star 1 (same shape)
+112  16  flare fade slots x4: { u16 light_id; u8 fade_ctr; u8 latched_tag }
+128  24  near-star slots x2: { u8 class; u8 rsv; u16 spin_phase;
          s32 radius_milli; u32 texture_seed }
+152   4  GALAXY_SEED (u32)
+156  12  camera sector (3 x s32)
+168  68  motion-trail rings x2 (stars_and_flares §15): { 8 x u16 x_px;
          8 x u16 y_px; u8 head; u8 length }
```

**[v3] ENVIRONMENT_STATE body (20 B — byte-mirror of the `SetEnvironment`
payload, so a captured frame's light state and the command that set it can
never drift):**

```
+0    2  sun_yaw (angle16, turns)
+2    2  sun_pitch (angle16, turns; 0x4000 = zenith)
+4    2  sun_colour (rgb565)
+6    2  ambient (rgb565)
+8    2  tint (rgb565)
+10   1  tint_strength (unit8, qformats §2)
+11   1  fog mode (fog_mode enum: 0 off, 1 linear)
+12   4  fog_near (fx16, world metres)
+16   4  fog_far (fx16, world metres)
```

**[v3] State-chunk replay-exactness law.** CELESTIAL_STATE and
ENVIRONMENT_STATE are the temporal state the §3 command stream alone
cannot reconstruct: palettes in flight, flare fade counters, trail
rings, the resolved light and fog. A capture that contains them replays
from ANY frame boundary bit-exactly — light, weather and trails
included — with no warm-up rule (a trail is history; the capture is the
history — stars §8's ruling, generalised here to the environment: a
storm is sim state, and state the machine consumes is captured, never
re-derived). Writers SHOULD emit both chunks per captured frame (as
CONTROLLER_SNAPSHOT is); readers MUST skip unknown section versions per
§4.3 rule 2. The crossfade weight of sky_and_beams §1.3 is deliberately
NOT a chunk field: its products (the palette upload, the resolved
SetEnvironment values) are command-stream bytes, and the capture records
what the machine consumes, not what the sim was thinking.

`ABI_INFO` identities: `zidl_sha256` = SHA-256 over the exact bytes of
`spec/commands.zidl`; `generator_sha256` = SHA-256 over the generator's
canonical LayoutIR text (emitted in `spec/generated/abi.md` as
`abi_identity_sha256`), so a capture pins both the source ABI and the layout
computation that produced its tables.

### 4.3 Evolution rules

1. Readers MUST skip unknown `section_type` values (length is known from the
   table) — tested behavior, not convention.
2. Known sections evolve via their own `section_version`; a reader that knows
   a type but not the version fails with a clear error — it never guesses.
3. `format_version` bumps only for header/section-table layout changes; the
   v1 layout never changes.
4. Sections may appear in any order except ABI_INFO-first; duplicate section
   types are an error unless entry flags say otherwise.
5. No whole-file CRC: per-section CRCs + header CRC give partial recovery of
   truncated captures.

### 4.4 Read/write API

- **Writer** (Phase 1: seekable files): create → header with placeholders →
  append section bodies sequentially, recording (type, version, offset,
  length, CRC) → write section table → backpatch header (section_count,
  total_file_length). CRCs computed incrementally (the 2 running form).
  Non-seekable (stdout) two-pass writing is deferred.
- **Reader**: open → validate magic/flags/header_crc32c → load section table
  → random access by type (no full-file scan) → per-section CRC verified on
  demand; iteration is streaming/chunked.

---

## 5. Source-ID scheme (used by `source_id` fields and SOURCE_MAP)

`source_id: u32 = { kind: 4 bits, module: 12 bits, index: 16 bits }`

- kinds: 0 NONE (bootstrap), 1 form declaration, 2 population, 3 field
  program, 4 material, 5 command site, 6 audio event, 7 stamp operation;
  **[w3]** 8 system, 9 presentation emit site, 10 pool, 11 scenario.
- 4096 modules (module IDs `0..4095`), with exactly 65536 source-producing
  rows available per module (local indices `0..65535`). The local index is
  **zero-based**: the first source-producing row in a module has index 0.
  Constants, enums, structs, globals, functions, sounds and presentations as
  containers do not consume rows. Pools, systems, field programs, every
  presentation `emit` statement, and scenarios do consume one row each.
  Structure is visible in traces (`kind=9, module=0x301, index=0x00AF`), which
  hashes are not.
- Allocation is a sequential registry, not hashing: compiler assigns `module`
  by canonical sort of module paths and assigns the zero-based local `index`
  in declaration/emit order across only the source-producing rows above — a
  rebuild of unchanged sources yields identical IDs. Admission rejects a
  4097th module or a 65537th source-producing row before lowering. Content
  identity travels separately as program hashes (sha-256 in RESOURCE_PAGES /
  CRC-32C per 1.B-7).
- Compiler sidecar `sourceids.zmap` — **[w3]** binary, format §7 (was JSON in
  Phase 1; D11): one entry per ID
  `{source_id, kind, file_index, span, name}`. At capture time the
  manifest is embedded as the SOURCE_MAP section so a .zcap is self-describing
  forever.

**[w3] Kind allocation note (deviation recorded):** plan D11 named the new
kinds "5=system, 6=presentation emit site, 7=pool, 8=scenario", but kinds
5-7 were already frozen in Phase 1 (command site / audio event / stamp
operation) and appear in committed goldens (`zcap_minimal.zcap` carries kind-5
command-site entries). The wave-3 kinds are therefore **appended as 8-11**;
no existing kind value changed meaning. The four new kinds attribute the
wave-3 language objects that own runtime costs: 8 = `system` declarations
(per-phase/rate cost rows), 9 = presentation emit statements (each lowers to
one ABI command template — the record `source_id` of an emitted command
points here), 10 = `pool` declarations (capacity/bandwidth rows), 11 =
`scenario` blocks (golden capture provenance).

SOURCE_MAP body: `u32 count` + count × `{u32 source_id; u16 module_id; u8 kind; u8 flags; u32 line; u16 name_off; u16 file_off}` + UTF-8 string blob (`name_off`/`file_off` are byte offsets into the blob, each string NUL-terminated).

Resolution order (inspector/tools): embedded SOURCE_MAP → sidecar for live
builds → raw hex display. Never a wrong guess.

---

## 6. Determinism and generation discipline

- `tools/abi-gen` output is deterministic and timestamp-free: identical .zidl
  ⇒ byte-identical outputs (`npm run abi:check` regenerates and diffs; stale
  generated files fail CI).
- Golden binaries under `tests/abi/golden/` (per-command records, minimal
  frame packet, minimal .zcap, fuzz corpus with expected error codes) are
  generated artifacts **and committed evidence**. C++ (ZRef), TS and the
  Verilated SV package each rebuild them independently and byte-compare —
  the protobuf-conformance pattern: one corpus, many testees, identical bytes
  required.
- Every validator (C++ `zref_frame`, TS `frame.ts`, SV `zhao_abi_pkg`) walks
  the 3.2 order; the fuzz corpus asserts tri-language error-code identity on
  malformed inputs.

### 6.1 Wave-2 golden capture naming ([v2], plan W2.7)

Per-mode 10-frame replays and the Duo marker trajectory live under
`captures/golden/wave2/`:

| File | Contents |
|---|---|
| `captures/golden/wave2/z60_10frame.zcap` | Z60, 10 sealed frames + per-frame FRAMEBUFFER_EXPECTED (displayed-CRC) + CONTROLLER_SNAPSHOT + COUNTERS |
| `captures/golden/wave2/storm_10frame.zcap` | same, Storm |
| `captures/golden/wave2/duo_10frame.zcap` | same, Duo (both view canvases) |
| `captures/golden/wave2/duo_markers.zcap` | 600-frame Duo marker demo: trajectory hash = CRC-32C chain over the 600 displayed-frame CRCs, recorded as a COUNTERS entry |

Every wave-2 capture records its timing profile version in ABI_INFO
generator identity (sim tables are provisional until ZH-016; plan R3), and
uses section_version = 1 for all sections above. FRAMEBUFFER_EXPECTED
sections are one per frame, in frame order.

---

## 7. `sourceids.zmap` — binary source-ID sidecar **[w3]** (D11)

The compiler-sidecar upgrade of the Phase-1 JSON map: one file per build,
embedded verbatim as the SOURCE_MAP section of captures (§5) and as a
page of the `.zpak` cartridge (spec/cartridge.md §2/§3, kind 1). Little-endian
throughout; CRC-32C per §2 parameterization.

### 7.1 File layout

```
Offset  Size  Field                Notes
0       4     magic 'Z','S','M','P'  (u32 LE 0x504D535A)
4       2     format_version = 1
6       2     flags                 bit0 = contains_program_hashes; bits 1-15 reserved 0
8       4     entry_count           u32
12      4     file_count            u32
16      4     string_blob_bytes     u32 (blob follows the tables)
20      4     body_crc32c           CRC-32C over ALL following bytes (entries + files + blob)
24      8     reserved              u64, must be 0
32      ..    entries: entry_count × 24-B records (7.2), source_id ascending
        ..    files:  file_count × 8-B records (7.3), file_index ascending
        ..    string blob: string_blob_bytes UTF-8, every string NUL-terminated
```

### 7.2 Entry record (24 B)

```
+0  u32  source_id          §5 scheme {kind:4, module:12, index:16}
+4  u16  file_index         index into the file table (7.3)
+6  u8   kind               the §5 kind (denormalized for tooling convenience;
+7  u8   flags              bit0 = has_program_hash; bits 1-7 reserved 0
                            must equal source_id's kind field — a mismatch is
                            a corrupt map)
+8  u32  span_begin         byte offset in the file (lexer law, language-semantics §1)
+12 u32  span_end           byte offset, exclusive; span_end >= span_begin
+16 u16  name_off           byte offset into the string blob
+18 u16  rsv                0
+20 u32  program_hash       CRC-32C over code+tables (field-ir §5.4);
                            0 unless flags bit0
```

Spans are the same `SourceSpan` byte offsets the parser attaches to every
AST node and the Field IR builder consumes (D2/D3) — the map is the
`source_id → {kind; file_index; span; name}` registry of D11, and a
field-program PC resolves to a span through field-ir §8 exactly as before.

### 7.3 File record (8 B)

```
+0  u32  path_off           byte offset into the string blob (module path)
+4  u32  rsv                0
```

Module identity order matches §5: `module` ids assigned by canonical sort of
module paths; the file table is written in that order so `file_index`
ordering equals module-id ordering.

### 7.4 Laws

- **Determinism:** the map is a pure function of the compiled sources —
  byte-identical across rebuilds (the canonicalization law of §6; asserted by
  `form:check`).
- **Ascending:** entries are sorted by `source_id` ascending; duplicates are
  a corrupt map (registry uniqueness, §5).
- **Integrity:** `body_crc32c` covers bytes [32, EOF); a mismatch refuses
  the whole map — never a partial resolution. The magic/version check comes
  first (fail-safe order, §3.2 discipline).
- **Resolution order** (inspector/tools, §5): embedded SOURCE_MAP → this
  sidecar for live builds → raw hex display. Never a wrong guess.
- **Phase-1 JSON compat:** the JSON sidecar remains readable by tools for
  old captures; new builds emit binary only.
