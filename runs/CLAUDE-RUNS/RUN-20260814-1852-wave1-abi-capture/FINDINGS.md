# FINDINGS — P5 Recon: Command ABI IDL Generator, Frame Packet, .zcap Capture Container

**Run:** RUN-20260814-1852-wave1-abi-capture
**Date:** 2026-08-14
**Agent:** RECON (no repo modification; persisted by orchestrator — subagent file-write blocked by harness policy)
**Inputs:** Charter v0.2 §7.4, §19, §20.5, §20.6, §23; START_HERE v0.2; FORM_LANGUAGE_HARDWARE_CODESIGN.md §17, §19.3

---

## 0. Executive summary

| Decision | Choice | Rationale (short) |
|---|---|---|
| Serialization scheme | Custom fixed-layout IDL (`.zidl`), one TS generator, three emitters | FlatBuffers/Cap'n Proto/protobuf are offset/varint-based, not register-map friendly; Kaitai is parse-only, no SV; see §1 |
| CRC | CRC-32C (Castagnoli), reflected 0x82F63B78, init/xorout 0xFFFFFFFF | HW instructions on x86 (SSE4.2) and ARMv8 (FEAT_CRC32); HD≥6 to ~654 B, HD≥4 to 2^31 bits; iSCSI-proven; see §2 |
| Byte order | Little-endian everywhere (incl. stored CRC words); reflected CRC | Charter rule; matches zlib-style stock implementations |
| Alignment | Natural alignment per field (cap 4), explicit visible padding, command records multiples of 16 B | RTL slices by field; no hidden C padding; 16-B matches charter & DMA burst friendliness |
| Command framing | Fixed 16-B command header `{u16 opcode, u16 record_bytes, u32 source_id, u32 flags, u32 reserved0}` + payload | Per-command length prefix enables fail-safe validation before decode |
| Frame packet | 32-B sealed header (magic, abi, flags, frame_id, sequence, resource_epoch, deadline, counts) + header CRC + command stream + trailing payload CRC | §7.4 seal = length + sequence + resource epoch + CRC |
| .zcap | Sectioned container: fixed header + section table + independently versioned, individually CRC-32C'd sections | Unknown-section skip rule gives evolution; random-access reads |
| Source IDs | 32-bit hierarchical `{kind:4, module:12, index:16}` from a sequential registry + sidecar/embedded manifest | Collision-free, debuggable, capture-stable across compiler changes |
| Generator | Single layout calculator (TS) → LayoutIR → emitters for C++/TS/SV/docs/fuzz | One source of truth; byte-identity enforced by golden vectors in CI |

---

## 1. IDL / codegen survey and the .zidl grammar

### 1.1 Why the off-the-shelf schemes don't fit

(Requirements: little-endian, explicit sizes, 16-byte alignment, versioned, register-map-friendly,
malformed commands fail safely, C++/TS/SV byte-identical.)

- **FlatBuffers** — tables use vtable indirection: a table starts with an offset to a shared vtable
  holding per-field offsets, so field positions are not compile-time constants and decoding chases
  two indirections. FlatBuffers *structs* are the closest analogue ("a consistent memory layout
  where all components are aligned to their size", stored inline, no versioning) — but scalar
  alignment caps at 8, no bitfield support, no 16-byte rule, and **no SystemVerilog backend**.
  [Internals](https://flatbuffers.dev/internals/overview.html), [White Paper](https://flatbuffers.dev/white_paper/)
- **Cap'n Proto** — fixed offsets per schema version, but every non-inline value is a *pointer*
  (30-bit word offsets, data + pointer sections, fields XOR'd with defaults), 8-byte word
  alignment only. Pointer-chasing is poison for an RTL decoder that wants to slice `data[63:48]`.
  [Encoding spec](https://capnproto.org/encoding.html)
- **Protocol Buffers** — base-128 varints: a field's byte position and length depend on its
  *value*; tag–value pairing, zigzag encoding, no alignment, no bitfields, no SV generation.
  Optimizes for schema *evolution*; Zhaozhou's ABI optimizes for *positional stability* so a
  decoder can slice bits by field. Opposite goals.
  [Wire format](https://protobuf.dev/programming-guides/encoding/)
- **Kaitai Struct** — one declarative YAML (`.ksy`) → parsers in C++/STL, C#, Go, Java, JS, Python,
  Ruby, Rust, etc. Right *shape*, wrong *job*: readers/parsers only, no writers, no validators,
  no register maps, and no HDL target at all (verified on kaitai.io). Useful later as inspiration
  for documenting the .zcap reader. [kaitai.io](https://kaitai.io/),
  [compiler](https://github.com/kaitai-io/kaitai_struct_compiler)
- **SystemRDL 2.0 / PeakRDL / ORDT / hdl-registers / RgGen** — register-map IDLs with C + SV
  exporters (SystemRDL is explicitly "a single source for register descriptions from which
  multiple output views — such as RTL code or documentation — can be automatically generated").
  They model *address-mapped registers*, not streamed command packets; wrong metaphors for the
  command ABI, but the natural candidates for `spec/registers.zidl` later.
  [SystemRDL 2.0](https://www.accellera.org/images/downloads/standards/systemrdl/SystemRDL_2.0_Jan2018.pdf),
  [PeakRDL](https://peakrdl.readthedocs.io/), [PeakRDL-regblock](https://peakrdl-regblock.readthedocs.io/),
  [ORDT](https://github.com/Juniper/open-register-design-tool),
  [hdl-registers](https://hdl-registers.com/)
- **Single-source multi-language precedents** — Vulkan's `vk.xml` registry → canonical C headers +
  bindings ([registry docs](https://registry.khronos.org/vulkan/specs/latest/registry.html),
  [vk.xml](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml));
  Mesa Intel **genxml** — XML instructions/structs/registers with explicit bit ranges → C
  pack/unpack headers (closest spiritual ancestor of commands.zidl)
  ([src/intel/genxml](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/intel/genxml));
  AMD's register JSON database in Mesa
  ([mergedbs.py](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/amd/registers)).
  Cross-implementation conformance precedent: protobuf's conformance runner — one runner, many
  testees, identical responses required
  ([upstream runner](https://github.com/protocolbuffers/protobuf/blob/main/conformance/conformance_test_runner.cc),
  [bufbuild conformance](https://github.com/bufbuild/protobuf-conformance)).
- **Alignment/padding** — implicit compiler padding differs by C ABI/word size (e.g. `double`
  8-aligned on Win64, 4-aligned on 32-bit Linux —
  [Wikipedia: Data structure alignment](https://en.wikipedia.org/wiki/Data_structure_alignment)),
  which is why the IDL must carry **explicit reserved/padding fields** and the generated C++
  must `static_assert` every offset. Power-of-two alignment keeps hardware masking cheap
  (`(offset + align-1) & -align`); 16 B matches a 128-bit command word / cache-line fraction and
  lets a fetcher use aligned bursts.
- **DPI-C** — for calling C++ from SV testbenches (stimulus, reference models, file I/O);
  orthogonal to codegen. Use it as the cosim glue between the C++ builder and generated SV
  packed structs, never to define layouts. [Verilator connecting/DPI](https://verilator.org/guide/latest/connecting.html)

**Conclusion:** a small purpose-built IDL with one generator and a shared layout calculator is the
correct weight for Phase 1 — exactly charter rule §29.5 ("Do not maintain command structs manually
in three languages; generate them"). Nothing off the shelf does the whole job, and every piece of
the approach has strong precedent.

### 1.2 .zidl grammar (EBNF)

Principles: fixed-size types only; every field offset computable by the layout calculator;
padding explicit and visible in generated output; enums carry explicit values; bitfields declared
inside a `bits` container so the RTL emitter generates exact `[hi:lo]` ranges; handles are
first-class with epoch semantics; versioning is file-level plus opcode-stability rules.

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

const_decl   = "const" , prim_type , ident , "=" , int ;

enum_decl    = "enum" , ident , [ ":" , prim_type ] , "{" , { enum_entry } , "}" ;
enum_entry   = ident , "=" , int , ";" ;

struct_decl  = "struct" , ident , "{" , { field } , "}" ;
               (* composed types: mat4fx, rectfx, transform2fx are builtin or user structs *)

command_decl = "command" , ident , hex_int , "{" , { field } , "}" ;
               (* hex_int = opcode; unique; ranges reserved — see §1.3 *)

field        = field_type , ident , [ array_suffix ] , ";" ;
field_type   = prim_type | handle_type | enum_ref | struct_ref | bits_decl ;

prim_type    = "u8" | "u16" | "u32" | "u64"
             | "i8" | "i16" | "i32" | "i64"
             | "fx16" | "fx32" | "fx64"          (* fixed point; Q formats per spec/qformats.md *)
             | "pad" ;                            (* one byte; use arrays: pad[12] *)

handle_type  = "handle32" , [ "[" , ident , "]" ] ;
               (* wire: u32 = { index:24 , generation:8 } ; optional resource-kind enum *)

enum_ref     = enum_decl_ident ;
struct_ref   = struct_decl_ident ;

bits_decl    = "bits" , prim_type , ident , "{" , { bit_member } , "}" ;
               (* container occupies width of prim_type (u8/u16/u32/u64);
                  members packed LSB-first in declaration order; generator rejects overflow;
                  'reserved' members emit SV localparams but no functional field *)
bit_member   = ident , [ ":" , int ] ;               (* default width 1 *)

array_suffix = "[" , int , "]" ;                     (* fixed-length arrays only in Phase 1 *)
```

Layout rules enforced by the generator (see §1.4):

1. Little-endian; field offsets accumulate in declaration order.
2. Every field of size S must start at an offset divisible by min(S, 4); padding must be supplied
   explicitly via `pad` fields — **no implicit padding anywhere**. The generator errors out
   (never silently inserts) so the .zidl is the complete truth.
3. Bitfield containers follow rule 2 for their whole width; members are LSB-first.
4. A command's total size must be a multiple of `command_alignment` (16); trailing pad to 16 is
   declared in the .zidl, not hidden.
5. Arrays multiply the element layout; element stride = element's padded size
   (so `u32 x[2]` is 8 bytes at a 4-aligned offset).
6. The charter's `bytes parameters[64]` is spelled `u8 parameters[64]` — one canonical byte-array
   spelling.

Versioning rules:

- `abi.version` gates the whole file; generated headers embed it; the frame header carries it.
- Opcodes are frozen once shipped: a command's opcode, field set, and sizes never change;
  additive change = new opcode (old kept as deprecated alias).
- Opcode ranges: `0x000n` frame control, `0x001n` views/presentation, `0x002n` terrain/surface,
  `0x003n` forms/populations, `0x004n` audio, `0xF00n` bootstrap/debug (never game-facing).

### 1.3 Initial Phase 1 command set

Every command record = fixed 16-B header (§3.2) + payload; totals below are record sizes.

| Command | Opcode | Record B | Payload after 16-B header | Status |
|---|---|---|---|---|
| `Nop` | 0x0000 | 16 | none | implement (decoder test vehicle) |
| `BeginFrame` | 0x0001 | 32 | `u32 frame_id; u32 resource_epoch; u32 flags; u32 deadline_cycles` | implement (charter) |
| `EndFrame` | 0x0002 | 32 | `u32 completion_flags; u32 expected_crc_valid; u32 expected_framebuffer_crc; pad[4]` | implement |
| `SetView` | 0x0010 | 96 | `u8 view_id; u8 viewport_id; u16 flags; mat4fx view_projection (64 B); fx16 pixel_error; u16 pad0; u32 geometry_tokens; u32 fragment_tokens; pad[8]` | implement (charter) |
| `SetPresentationContract` | 0x0020 | 48 | `u8 mode; u8 view_count; u16 flags; u32 geometry_tokens[2]; u32 fragment_tokens[2]; u32 shared_tokens; pad[4]` | implement (charter) |
| `TerrainField` (= APPLY_TERRAIN_FIELD) | 0x0200 | 96 | `handle32[program] program; rectfx footprint (16 B); u32 start_tick; u32 duration_ticks; u8 parameters[64]` | reserved (semantic) |
| `SurfaceStamp` (= STAMP_SURFACE) | 0x0210 | 48 | `handle32[brush] brush; handle32[patch] patch; u8 operation; u8 tag; u16 strength; transform2fx transform (16 B); pad[4]` | reserved |
| `DrawForm` (= DRAW_FORM) | 0x0300 | 32 | `handle32[form] form; handle32[material_set] material_set; handle32[transform] transform; u8 viewport_mask; u8 semantic_weight; u16 flags` | reserved |
| `DrawPopulation` | 0x0301 | 32 | `handle32[population] population; u8 viewport_mask; u8 semantic_weight; u16 flags; pad[20]` | reserved |
| `DrawProcedural` (= DRAW_PROCEDURAL) | 0x0302 | 48 | `handle32[forge_program] program; handle32[material] material; transform2fx transform; fx16 screen_error; u16 pad0; pad[8]` | reserved |
| `EmitAudioEvent` | 0x0400 | 32 | `u32 event_id; i16 pan_fx; u16 gain; u32 sample_handle; u32 timestamp; pad[12]` | reserved |
| `DebugBootstrap` | 0xF001 | 64 | `u8 data[48]` (opaque; DRAW_TILE_WORK / DRAW_SCREEN_TRIANGLES umbrella) | reserved, bootstrap only |

`handle32` wire format: `{ index:24, generation:8 }`. Consumers compare `generation` against the
current epoch's generation and reject stale handles with `ZH_ABI_STALE_HANDLE` (charter:
"resource handles include generation/epoch checks").

### 1.4 Layout algorithm (shared, generator-internal)

```
off = 0
for each field:
    a = min(size_of(field), 4)            # natural alignment capped at 4
    if off % a != 0: ERROR "insert explicit pad[n]" (n = a - off % a)
    field.offset = off; off += size_of(field)
cmd: if off % 16 != 0: ERROR "pad command to 16 with pad[n]"
```

The calculator produces a **LayoutIR** table per command:
`{opcode, name, size, fields: [{name, type, offset, bits[hi:lo], enum, count}]}`. The three
emitters consume only this IR — they never re-derive layout, which makes byte-identity
structural rather than lucky.

### 1.5 Generator architecture (tools/abi-gen)

```
spec/commands.zidl
   │  parse (hand-written recursive-descent lexer/parser, zero deps)
   ▼
ZidlAst ── semantic pass (name resolution, enum/bitfield/opcode checks,
   │       alignment audit, opcode-range policy) → hard-fail errors
   ▼
LayoutIR (single layout calculator; deterministic; §1.4)
   ├──► emit_cpp  → runtime/include/zhao_abi.h        (C++17: structs, static_asserts on offsets, reader/writer, validator, opcode enum, CRC)
   ├──► emit_ts   → compiler/src/generated/abi.ts     (TS: const tables, DataView writer/reader, validator, CRC)
   ├──► emit_sv   → fpga/rtl/generated/zhao_abi_pkg.sv (SV package: packed structs, localparams, decoder fn, CRC fn)
   ├──► emit_doc  → spec/generated/abi.md             (byte maps; the Markdown ABI documentation charter §19 requires)
   └──► emit_fuzz → tests/fuzz/abi_corpus_gen.ts      (valid + malformed command generators)
```

Generation is deterministic and timestamp-free: identical .zidl ⇒ byte-identical outputs
(canonical ordering, fixed templates). CI runs the generator twice and diffs; a `--check` mode
fails if generated files are stale relative to `spec/commands.zidl` (golden-file testing, per
https://matttproud.com/blog/posts/golden-file-testing.html).

Emitter requirements:

- **C++**: no `#pragma pack` — structs contain only explicitly padded members so natural layout
  equals wire layout; `static_assert(offsetof(...) == N)` for every field; `Writer` appends
  little-endian into a byte arena; `Validator` returns `ZhAbiError` codes — never throws, never
  writes on failure.
- **TS**: single module; `Uint8Array` + `DataView(littleEndian=true)`; identical validator
  returning the same error-code integers as C++/SV.
- **SV**: `package zhao_abi_pkg;` with `typedef struct packed {...}` in **reverse field order**
  (SV packs MSB-first, so the first .zidl field must occupy the highest bits — the emitter
  inverts order; this is the top silent byte-identity hazard, hence golden vectors);
  `localparam` per opcode/offset/size; `function automatic logic [31:0] zhao_crc32c(...)`;
  a `zhao_command_validate` function returning `(error_code, record)`. Packed typedefs are
  directly usable as ready/valid payload types.

---

## 2. CRC specification

### 2.1 Choice: CRC-32C (Castagnoli)

Parameter set = **CRC-32/ISCSI** from the RevEng catalogue:
poly `0x11EDC641` (reflected `0x82F63B78`), init `0xFFFFFFFF`, refin=true, refout=true,
xorout `0xFFFFFFFF`, check `"123456789"` → `0xE3069283`, residue `0xB798B438`
([catalogue](https://reveng.sourceforge.io/crc-catalogue/17plus.htm)).

Why not IEEE CRC-32 (0x04C11DB7): Koopman's CRC Zoo shows CRC-32C holds Hamming distance 6 up to
5,243 data bits (~654 B) and distance 4 up to 2^31 bits, while IEEE CRC-32 drops to HD 4 past
~33 B and HD 3 past ~11.4 KB — exactly the regime of multi-KB frame packets
([CRC Zoo](https://users.ece.cmu.edu/~koopman/crc32.html),
[length table CRC-32C](https://users.ece.cmu.edu/~koopman/crc32/0x8f6e37a0_len.txt),
[length table IEEE](https://users.ece.cmu.edu/~koopman/crc32/0x82608edb_len.txt)).
This is why iSCSI adopted it ([RFC 3385](https://www.rfc-editor.org/rfc/rfc3385),
[Castagnoli93] cited in [RFC 3720](https://www.rfc-editor.org/rfc/rfc3720)).

Hardware: x86 SSE4.2 `CRC32` computes CRC-32C directly; ARMv8 FEAT_CRC32 exposes both families —
`CRC32B/H/W/X` (IEEE) and `CRC32CB/CH/W/CX` (Castagnoli) — so CRC-32C is hardware-accelerated on
both host families ([ARM ARM CRC32](https://developer.arm.com/documentation/ddi0602/2026-03/Base-Instructions/CRC32B--CRC32H--CRC32W--CRC32X--CRC32-checksum-),
[Wikipedia: CRC](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)). On FPGA, both
polynomials cost the same (byte-serial LFSR XOR network, or slicing-by-N unroll) — so choose on
detection strength, and CRC-32C wins.

### 2.2 Bit/byte-order conventions

"Reflected" = reverse bit order; we use the reversed polynomial `0x82F63B78` and shift the
register right, LSB-first — the zlib heritage (UARTs shipped LSB-first; "the reflection of good
polys tend to be good polys too" —
[Ross Williams, Painless Guide to CRC](https://zlib.net/crc_v3.txt),
[Sunshine, Understanding CRC](https://www.sunshine2k.de/articles/coding/crc/understanding_crc.html)).
Mathematically reflect-first vs reflect-last are the same computation; interop only requires
matching all five parameters (width/poly/init/refin+refout/xorout). Both SSE4.2 and ARM
instructions are the reflected, init/xorout 0xFFFFFFFF form — so our reference matches stock
hardware. **CRC values are stored little-endian** as u32 words (matches the ABI-wide convention;
RFC 3720 §12.1 devotes a bullet list to CRC byte mapping because it is the #1 interop bug class —
we document it here as a rule).

### 2.3 Reference implementation (identical in all three languages)

Table-driven (zlib style), C first; TS identical modulo types (table emitted as a literal const
for determinism); SV byte-serial step in §2.5.

```c
// CRC-32C: poly 0x82F63B78 (reflected), init 0xFFFFFFFF, xorout 0xFFFFFFFF
static uint32_t crc32c_table[256];            // abi-gen emits this table as a literal
static void crc32c_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0x82F63B78 & (uint32_t)(-(int32_t)(c & 1)));
        crc32c_table[i] = c;
    }
}
uint32_t crc32c(uint32_t crc, const void *buf, size_t len) {   // crc = running value or 0
    const uint8_t *p = buf;
    crc = ~crc;                                          // absorb init on incremental calls
    while (len--)
        crc = crc32c_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;                                         // final xorout
}
```

Fast paths (optional, behind a capability check, must produce identical results):
x86 `_mm_crc32_u8/u32/u64`; AArch64 `__crc32cb/cw/cx`. On FPGA, byte-serial LFSR or unrolled
slicing-by-N ([Büsch CRC generator](https://bues.ch/crcgen),
[PXVI parallel CRC](https://github.com/PXVI/ip_parallel_custom_crc_gerator_verilog),
[slice-by-N analysis](https://is.muni.cz/th/b7glm/crc.pdf)).

### 2.4 Test vectors

| Input | CRC-32C |
|---|---|
| `"123456789"` (ASCII, 9 B) | `0xE3069283` |
| 32 × `0x00` | `0x8A9136AA` (wire `aa 36 91 8a` per RFC 3720 B.4) |
| 32 × `0xFF` | `0x62A8AB43` |
| 32 B `0x00..0x1F` | `0x46DD794E` |
| 32 B `0x1F..0x00` | `0x113FDB5C` |
| empty (0 B) | `0x00000000` (derived: init ⊕ xorout = 0; verify in-suite) |
| residue: CRC over (message ‖ CRC) | `0x1C2D19ED` (RTL self-test) |

Sources: [RFC 3720 Appendix B.4](https://www.rfc-editor.org/rfc/rfc3720),
[RevEng catalogue](https://reveng.sourceforge.io/crc-catalogue/17plus.htm),
[SO: CRC32C test vectors](https://stackoverflow.com/questions/20963944/test-vectors-for-crc32c).
(For contrast, IEEE CRC-32 of "123456789" is `0xCBF43926`.)

### 2.5 SystemVerilog synthesizable step

```systemverilog
function automatic logic [31:0] zhao_crc32c_step(input logic [31:0] c,
                                                 input logic [7:0]  d);
    logic [31:0] crc = c ^ {24'b0, d};   // reflected: byte enters at LSB
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);
    return crc;
endfunction
// frame: seed 32'hFFFFFFFF, one step per byte, result = ~crc
```

The per-byte invariant `crc' = table[(crc^byte)&0xFF] ^ (crc>>8)` is the cross-language
contract: one golden-vector file validates C++, TS and SV against each other.

### 2.6 Capture-tool verification

Tools verify each .zcap section and each frame packet independently: read body → compute CRC-32C
→ compare stored little-endian word; on mismatch report section type + offset, never trust
contents. The residue check (`message‖CRC` → `0x1C2D19ED`) doubles as a running-RTL self-test.
iSCSI's precedent of separating header and data digests motivates our frame header CRC split
(RFC 3720 §10.2.3/§12.1). pcapng carries no block CRCs at all (only duplicated block lengths for
backward navigation, [pcapng draft](https://datatracker.ietf.org/doc/html/draft-tuexen-opsawg-pcapng))
— our per-section CRCs are strictly stronger and permit partial recovery of truncated captures.

---

## 3. Frame packet layout

### 3.1 Sealed packet (one frame slot, HPS DDR ring)

```
Offset  Size  Field                Notes
0       4     magic                'Z','P','K','1'  (0x31504B5A as u32 LE)
4       2     abi_version          u16; must equal generated ABI version
6       2     flags                u16; bit0 = contains_debug_commands; rest 0 (must be 0)
8       4     frame_id             u32
12      4     sequence             u32  (monotonic packet sequence)
16      4     resource_epoch       u32  (charter §7.4 seal component)
20      4     deadline_cycles      u32
24      4     command_count        u32  (records in stream)
28      4     command_bytes        u32  (stream length; multiple of 16)
32      4     header_crc32c        CRC-32C over bytes [0,32)
36      N     command stream       N = command_bytes; sequence of records (§3.2)
36+N    4     payload_crc32c       CRC-32C over the command stream
```

Total size = 40 + N, N ≡ 0 (mod 16). Dual-CRC rationale: the decoder validates the 32-byte
header *before* trusting `command_bytes`/`command_count`; a corrupted length is caught by
`header_crc32c` rather than causing a wild read. (Alternative — single trailing CRC over
everything, Ethernet-style — was rejected because the header must be trustworthy enough to bound
the payload read.) Seal protocol per §7.4: producer writes header + stream + payload CRC +
header CRC last, then commits the slot ARM_WRITING → READY; the slot is never touched again; the
FPGA never exposes a partially consumed slot.

### 3.2 Command record framing

```
Offset  Size  Field          Notes
0       2     opcode         u16; must exist in the ABI table
2       2     record_bytes   u16; total record size incl. header; multiple of 16; >= 16
4       4     source_id      u32; §5 scheme; 0x00000000 = ZH_SOURCE_NONE (bootstrap)
8       4     flags          u32; defined bits per command; undefined bits MUST be 0
12      4     reserved0      u32; MUST be 0
16      ..    payload        (record_bytes - 16) bytes, per LayoutIR
```

Byte map, `BeginFrame` record (32 B total):

```
+0  u16 0x0001  opcode
+2  u16 32      record_bytes
+4  u32         source_id
+8  u32         flags
+12 u32 0       reserved0
+16 u32         frame_id
+20 u32         resource_epoch
+24 u32         flags2 (begin-frame flags)
+28 u32         deadline_cycles
```

### 3.3 Fail-safe validation ("malformed commands fail safely")

Validation order — every check runs **before** any payload field is consumed:

1. magic, abi_version, flags-reserved-bits → `ZH_ABI_BAD_MAGIC` / `BAD_ABI_VERSION` / `RESERVED_FLAG`
2. Bounds: `36 + command_bytes + 4 <= slot_size`, `command_bytes % 16 == 0`,
   `command_count * 16 <= command_bytes` → `ZH_ABI_BAD_LENGTH`
3. `header_crc32c` → `ZH_ABI_BAD_HEADER_CRC`
4. `payload_crc32c` → `ZH_ABI_BAD_PAYLOAD_CRC`
5. Per record: `record_bytes % 16 == 0`, `>= 16`, running sum ≤ `command_bytes`,
   opcode known, `record_bytes == LayoutIR[opcode].size` → `ZH_ABI_BAD_LENGTH` /
   `ZH_ABI_UNKNOWN_OPCODE`
6. Reserved fields zero, flags bits defined → `ZH_ABI_RESERVED_FIELD` / `ZH_ABI_RESERVED_FLAG`
7. Enum ranges, enum-typed u8 fields within declared enum → `ZH_ABI_BAD_VALUE`
8. Handle generations match `resource_epoch` → `ZH_ABI_STALE_HANDLE`
9. Truncation (sum of records < command_bytes) → `ZH_ABI_TRUNCATED`

Error codes are a generated enum shared verbatim across C++/TS/SV (`ZH_ABI_OK = 0`, then the
above). On any error the frame aborts: the slot transitions to DONE with the error code recorded
(counters + trace), no partial consumption, no writes outside assigned memory — matching charter
§20.4's formal property "malformed commands cannot write outside assigned memory".

---

## 4. .zcap container

### 4.1 File layout

```
Offset  Size  Field
0       4     magic 'Z','C','A','P'  (0x5041435A LE)
4       2     format_version = 1
6       2     flags               bit0 = little_endian (must be 1 in v1); rest reserved 0
8       4     header_crc32c       CRC-32C over bytes [0,8)
12      4     section_count       u32
16      4     section_table_offset = 32
20      4     section_entry_size = 32
24      8     total_file_length   u64
32      ..    section table: section_count × 32-B entries

Section-table entry:
+0  u16  section_type
+2  u16  section_version        (independent per section type)
+4  u16  flags                  bit0 = crc_present; rest 0
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
| 0x0002 | FRAME_PACKET | raw sealed frame packet bytes (§3.1) |
| 0x0003 | RESOURCE_PAGES | `u32 count` + count × `{u8 kind; u8 rsv[3]; u32 page_id; u64 byte_length; u8 sha256[32]; u8 ref[64]}` (page refs + hashes; no bulk data) |
| 0x0004 | CONTROLLER_SNAPSHOT | `u32 count` + count × PadFrame `{u8 pad_index; u8 flags; u16 sequence; u32 buttons; i16 lx, ly, rx, ry; u16 rsv}` (charter §17 canonical snapshot) |
| 0x0005 | FRAMEBUFFER_EXPECTED | `{u8 mode; u8 view_count; u16 flags; u16 width; u16 height; u16 rsv; u32 expected_crc32c}` |
| 0x0006 | TILE_CRC | `u32 count` + count × `{u32 tile_index; u32 crc32c}` |
| 0x0007 | DEPTH_STENCIL_CRC | optional; same shape as TILE_CRC |
| 0x0008 | COUNTERS | `u32 count` + count × `{u16 counter_id; u16 rsv; u64 expected_value}` |
| 0x0009 | SOURCE_MAP | `u32 count` + count × `{u32 source_id; u16 module_id; u8 kind; u8 flags; u32 line; u16 name_off; u16 file_off}` + UTF-8 string blob (offsets into blob) |
| 0x000A | TRACE | first-divergence record (charter §20.6): `{u32 tile; u32 primitive; u32 pixel; u8 stage; u8 rsv[3]; u32 expected_fx; u32 actual_fx; u32 source_id; u32 command_seq}` |
| 0x8000–0xFFFF | tool-namespace | tools may add private sections; readers must skip |

### 4.3 Versioning / evolution rules

1. Readers MUST skip unknown `section_type` values (length is known from the table) — tested
   behavior, not a convention.
2. Known sections evolve via their own `section_version`; a reader that knows a type but not the
   version fails with a clear error — it never guesses.
3. `format_version` bumps only for header/section-table layout changes; the v1 layout never
   changes.
4. Sections may appear in any order except ABI_INFO-first; duplicate section types are an error
   unless the entry flags say otherwise.
5. No whole-file CRC: per-section CRCs + header CRC give partial recovery of truncated captures
   (pcapng carries none at all; we are strictly stronger).

### 4.4 Streaming read/write API

- **Writer** (Phase 1: seekable files): create → write header with placeholders → append section
  bodies sequentially, recording (type, version, offset, length, CRC) → write section table →
  backpatch header (section_count, total_file_length). All CRCs computed incrementally
  (the §2.3 running form). Non-seekable (stdout) two-pass writing deferred to a later version.
- **Reader**: open → validate magic/flags/`header_crc32c` → load section table → random access
  by type (no full-file scan) → per-section CRC verified on demand; iteration is streaming and
  chunked so multi-GB future sections never require whole-file buffering.

---

## 5. Source-ID scheme

### 5.1 Allocation: sequential registry, not hashing

`source_id: u32 = { kind:4, module:12, index:16 }`

- kinds: 0 = NONE (bootstrap), 1 = form declaration, 2 = population, 3 = field program,
  4 = material, 5 = command site, 6 = audio event, 7 = stamp operation.
- 4,096 modules; 65,536 declarations per module — ample, and the structure is visible in traces
  (`0x0301AF` = kind 0, module 0x301, decl 0xAF) which hashes are not.

A 32-bit content hash was rejected: birthday collisions become likely around ~65 K declarations,
IDs churn under refactors, and hex traces are unreadable. Content identity already exists
elsewhere: program hashes (sha-256) travel in RESOURCE_PAGES/ABI_INFO, separate from source IDs.

Determinism: the compiler assigns `module` IDs by canonical sort of module paths and `index` in
declaration order, so a rebuild of unchanged sources yields identical IDs. Golden captures'
expected IDs are diffed in CI against the sidecar to catch accidental renumbering.

### 5.2 Sidecar manifest and capture stability

- Compiler emits `sourceids.zmap` (JSON in Phase 1):
  `{id, kind, module, file, line, name, program_hash?}` per entry.
- At capture time the manifest is embedded as the SOURCE_MAP section, so a `.zcap` is
  self-describing forever — it replays correctly even after compiler changes rename or reorder
  declarations.
- The inspector resolves: embedded SOURCE_MAP first → sidecar for live builds → raw hex display
  when neither matches (never a wrong guess). Phase 1 gate "source IDs and program hashes survive
  capture round-trips" is tested by capture → reload → resolve → compare.

---

## 6. Test plan

1. **Byte-identity matrix (Phase 1 gate):** abi-gen emits golden binaries
   `tests/abi/golden/*.bin` (each command + a full minimal frame packet + a minimal .zcap).
   C++ gtest, TS (node test runner) and a Verilator-linked SV testbench each *generate* the same
   artifacts and byte-compare against the golden set (protobuf-conformance pattern: one corpus,
   many testees, identical bytes required).
2. **CRC conformance:** §2.4 vectors in all three languages; residue self-test in RTL.
3. **Layout static checks:** C++ `static_assert(offsetof(...)==N && sizeof(Cmd)==M)`; SV
   `assert ($bits(typedef) == 8*M)`; TS runtime layout-table check against golden offsets.
4. **Round-trips:** write→read→deep-compare per language (commands, frame packets, .zcap);
   source-map resolution survives; program hashes unchanged.
5. **Fuzz (fail-safely):** generated corpus of malformed inputs — truncated records, bad
   opcodes, non-16-multiple lengths, length overshoot, nonzero reserved, stale handle
   generations, corrupted header/payload CRCs, unknown section types (must skip), wrong section
   versions (must reject cleanly). All three validators must return **identical error codes**;
   C++ under ASan/UBSan proves no out-of-arena writes; RTL guarded by assertions + the §20.4
   formal property.
6. **Generator determinism:** generate twice → diff; `--check` mode in every-commit CI so stale
   generated files fail the build.
7. **Empty-frame replay (Phase 1 gate):** a BeginFrame/Nop/EndFrame packet replays through empty
   ZRef and the stub RTL harness; completion, error codes and counters agree; CRC semantics match.

---

## 7. Cited URLs

IDL/codegen:
- https://flatbuffers.dev/internals/overview.html ; https://flatbuffers.dev/white_paper/
- https://capnproto.org/encoding.html
- https://protobuf.dev/programming-guides/encoding/
- https://kaitai.io/ ; https://github.com/kaitai-io/kaitai_struct_compiler
- https://www.accellera.org/images/downloads/standards/systemrdl/SystemRDL_2.0_Jan2018.pdf
- https://peakrdl.readthedocs.io/ ; https://peakrdl-regblock.readthedocs.io/
- https://github.com/Juniper/open-register-design-tool ; https://hdl-registers.com/
- https://registry.khronos.org/vulkan/specs/latest/registry.html
- https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/intel/genxml
- https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/amd/registers
- https://github.com/protocolbuffers/protobuf/blob/main/conformance/conformance_test_runner.cc
- https://github.com/bufbuild/protobuf-conformance
- https://en.wikipedia.org/wiki/Data_structure_alignment
- https://verilator.org/guide/latest/connecting.html
- https://matttproud.com/blog/posts/golden-file-testing.html

CRC:
- https://reveng.sourceforge.io/crc-catalogue/17plus.htm
- https://zlib.net/crc_v3.txt (Ross Williams, Painless Guide)
- https://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
- https://users.ece.cmu.edu/~koopman/crc/crc32.html ; .../c32/0x8f6e37a0_len.txt ; .../c32/0x82608edb_len.txt
- https://www.rfc-editor.org/rfc/rfc3720 (B.4 vectors, §10.2.3, §12.1) ; https://www.rfc-editor.org/rfc/rfc3385
- https://developer.arm.com/documentation/ddi0602/2026-03/Base-Instructions/CRC32B--CRC32H--CRC32W--CRC32X--CRC32-checksum-
- https://en.wikipedia.org/wiki/Cyclic_redundancy_check
- https://stackoverflow.com/questions/20963944/test-vectors-for-crc32c ; https://github.com/google/crc32c
- https://bues.ch/h/crcgen ; https://github.com/PXVI/ip_parallel_custom_crc_gerator_verilog
- https://is.muni.cz/th/b7glm/crc.pdf
- https://datatracker.ietf.org/doc/html/draft-tuexen-opsawg-pcapng

Flagged unverified during research: "roitgen" AMD register-JSON upstream repo (blocked),
"rusticlal" (not found — likely misremembered), OutputLogic.com CRC generator (TLS failure),
zero-length CRC check value (derived, verify in test suite).
