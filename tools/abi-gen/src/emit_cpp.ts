// emit_cpp.ts — runtime/include/zhao_abi.h emitter (P5 1.5). C++17, no
// #pragma pack (structs carry only explicitly padded members, so natural
// layout == wire layout), static_assert on every offset, LE writer/reader,
// CRC-32C table as a literal, sample records, command table.

import { CRC32C_TABLE } from './crc32c.js';
import { sampleCommand, structSample } from './sample.js';
import { CommandIR, FieldIR, LayoutIR, StructIR, upperSnake } from './types.js';

function cppPrim(t: string): string {
  switch (t) {
    case 'u8': return 'uint8_t';
    case 'u16': return 'uint16_t';
    case 'u32': return 'uint32_t';
    case 'u64': return 'uint64_t';
    case 'i8': return 'int8_t';
    case 'i16': return 'int16_t';
    case 'i32': return 'int32_t';
    case 'i64': return 'int64_t';
    case 'fx16': return 'int32_t'; // Q16.16 in a 32-bit container (qformats.md)
    case 'fx32': return 'int64_t'; // Q32.32 in a 64-bit container
    case 'angle16': return 'uint16_t'; // U 0.0.16 turns (qformats.md 2)
    default: return t;
  }
}

function cppValue(v: number, prim: string): string {
  if (prim === 'u64') return `${v}ull`;
  if (prim === 'i64') return `${v}ll`;
  const unsigned = prim.startsWith('u');
  return `${v}${unsigned ? 'u' : ''}`;
}

const structName = (s: string) => `Zh${s.charAt(0).toUpperCase()}${s.slice(1)}`;
const recordName = (c: CommandIR) => `ZhRecord${c.name}`;
const payloadName = (c: CommandIR) => `ZhCmd${c.name}`;

function declField(f: FieldIR, owner: string): string {
  const arr = f.count > 1 ? `[${f.count}]` : '';
  if (f.kind === 'pad') return `  uint8_t ${f.name}${arr};`;
  if (f.kind === 'handle') {
    return `  uint32_t ${f.name}${arr};  // handle32 {index:24, generation:8}${f.handleKind ? ` kind=${f.handleKind}` : ''}`;
  }
  if (f.kind === 'enum') return `  ${f.type} ${f.name}${arr};  // enum, ${f.size / f.count} B`;
  if (f.kind === 'struct') return `  ${structName(f.type)} ${f.name}${arr};`;
  return `  ${cppPrim(f.type)} ${f.name}${arr};`;
}

function writeScalar(prim: string, expr: string): string {
  switch (prim) {
    case 'u8': case 'i8': return `w.u8(${expr});`;
    case 'u16': case 'i16': case 'angle16': return `w.u16(${expr});`;
    case 'u32': case 'i32': case 'fx16': return `w.u32(${expr});`;
    case 'u64': case 'i64': case 'fx32': return `w.u64(${expr});`;
    default: return `w.u32(${expr});`;
  }
}

function readScalar(prim: string, expr: string, castTo?: string): string {
  const dst = castTo ? `${expr} = static_cast<${castTo}>(t)` : `${expr} = t`;
  switch (prim) {
    case 'u8': case 'i8': return `{ uint8_t t; if (!r.take8(t)) return false; ${dst}; }`;
    case 'u16': case 'i16': case 'angle16': return `{ uint16_t t; if (!r.take16(t)) return false; ${dst}; }`;
    case 'u32': case 'i32': case 'fx16': return `{ uint32_t t; if (!r.take32(t)) return false; ${dst}; }`;
    default: return `{ uint64_t t; if (!r.take64(t)) return false; ${dst}; }`;
  }
}

function hexBytes(hex: string): string {
  const out: string[] = [];
  for (let i = 0; i < 64; i += 2) out.push(`0x${hex.slice(i, i + 2).toUpperCase()}`);
  return out.join(', ');
}

export function emitCpp(ir: LayoutIR, identitySha256: string, zidlSha256: string): string {
  const L: string[] = [];
  // wire prim of a field: handles are u32; enums ride their BACKING prim
  // (the typed C++ member is the enum, but the wire width is the backing type)
  const primOf = (f: FieldIR) => (f.kind === 'handle' ? 'u32'
    : f.kind === 'enum' ? f.leaves[0]!.prim : f.type);

  // which structs are reachable from commands (emit samples/packs for those)
  const usedStructs = new Set<string>();
  const collect = (fields: readonly FieldIR[]) => {
    for (const f of fields) {
      if (f.kind === 'struct') {
        usedStructs.add(f.type);
        collect(ir.structs.get(f.type)!.fields);
      }
    }
  };
  for (const c of ir.commands) collect(c.fields);

  L.push('// GENERATED FILE - DO NOT EDIT');
  L.push('// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).');
  L.push('// Law: spec/capture_format.md. Identity (see spec/generated/abi.md):');
  L.push(`//   abi_identity_sha256 = ${identitySha256}`);
  L.push(`//   zidl_sha256         = ${zidlSha256}`);
  L.push('#pragma once');
  L.push('');
  L.push('#include <cstdint>');
  L.push('#include <cstddef>  // offsetof');
  L.push('#include <vector>');
  L.push('');
  L.push('namespace zhao_abi {');
  L.push('');

  // ---- ABI-level constants ---------------------------------------------------
  L.push(`constexpr uint16_t ZHAO_ABI_VERSION        = ${ir.abi.version};`);
  L.push(`constexpr uint16_t ZHAO_COMMAND_ALIGNMENT = ${ir.abi.commandAlignment};`);
  L.push('constexpr uint16_t ZHAO_OPCODE_WIDTH      = 2; // u16');
  for (const c of ir.consts) {
    L.push(`constexpr ${cppPrim(c.type)} ${c.name} = ${cppValue(c.value, c.type)};`);
  }
  L.push('');

  // ---- error enum (shared verbatim across C++/TS/SV) ---------------------------
  const errEnum = ir.enums.find((e) => e.name === 'zhao_abi_error');
  if (errEnum) {
    L.push(`enum ${errEnum.name} : uint32_t {`);
    for (const e of errEnum.entries) L.push(`  ${e.name} = ${e.value},`);
    L.push('};');
    L.push('');
  }

  // ---- value enums (ABI v2: typed members with their backing wire width) --------
  for (const e of ir.enums) {
    if (e.name === 'zhao_abi_error') continue;
    L.push(`// enum ${e.name}: ${e.type} on the wire (capture_format.md 3.2 step 7)`);
    L.push(`enum ${e.name} : ${cppPrim(e.type)} {`);
    for (const entry of e.entries) L.push(`  ${entry.name} = ${entry.value},`);
    L.push('};');
    L.push('');
  }

  // ---- opcodes ------------------------------------------------------------------
  for (const c of ir.commands) {
    L.push(`constexpr uint16_t ZHAO_OP_${upperSnake(c.name)} = ${c.opcodeHex}; // ${c.recordBytes} B, ${c.implemented ? 'implemented' : 'reserved'}`);
  }
  L.push('');

  // ---- frame packet constants (capture_format.md 3) ------------------------------
  L.push(`constexpr uint32_t ZHAO_FRAME_MAGIC        = 0x314B505Au; // 'Z','P','K','1' LE`);
  L.push('constexpr uint32_t ZHAO_FRAME_HEADER_BYTES = 36;');
  L.push('constexpr uint32_t ZHAO_FRAME_OVERHEAD     = 40;  // header + payload_crc32c');
  L.push('constexpr uint16_t ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 0x0001;');
  L.push('constexpr uint8_t  ZHAO_COMPL_DONE = 0x01; // completion_flags output bit');
  L.push('constexpr uint8_t  ZHAO_COMPL_ERR  = 0x02;');
  for (const [nm, off] of [['MAGIC', 0], ['ABI_VERSION', 4], ['FLAGS', 6], ['FRAME_ID', 8],
    ['SEQUENCE', 12], ['RESOURCE_EPOCH', 16], ['DEADLINE', 20], ['COMMAND_COUNT', 24],
    ['COMMAND_BYTES', 28], ['HEADER_CRC', 32]] as const) {
    L.push(`constexpr uint32_t ZHAO_OFF_${nm.padEnd(14)} = ${off};`);
  }
  L.push('');

  // ---- source-id scheme (capture_format.md 5) ------------------------------------
  L.push('constexpr uint32_t ZHAO_SOURCE_KIND_NONE = 0;');
  L.push('constexpr uint32_t ZHAO_SOURCE_KIND_COMMAND_SITE = 5;');
  L.push('inline uint32_t zhao_source_id_encode(uint32_t kind, uint32_t module, uint32_t index) {');
  L.push('  return (kind << 28) | (module << 16) | index;');
  L.push('}');
  L.push('inline void zhao_source_id_decode(uint32_t id, uint32_t& kind, uint32_t& module, uint32_t& index) {');
  L.push('  kind = id >> 28; module = (id >> 16) & 0xFFF; index = id & 0xFFFF;');
  L.push('}');
  L.push('');

  // ---- CRC-32C (capture_format.md 2) ----------------------------------------------
  L.push('// CRC-32C (Castagnoli): poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF.');
  L.push('constexpr uint32_t ZHAO_CRC32C_TABLE[256] = {');
  for (let i = 0; i < 256; i += 4) {
    L.push(`  ${[0, 1, 2, 3].map((k) => `0x${(CRC32C_TABLE[i + k]!).toString(16).toUpperCase().padStart(8, '0')}`).join(', ')},`);
  }
  L.push('};');
  L.push('inline uint32_t zhao_crc32c(uint32_t crc, const void* buf, size_t len) {');
  L.push('  const uint8_t* p = static_cast<const uint8_t*>(buf);');
  L.push('  crc = ~crc;');
  L.push('  while (len--) crc = ZHAO_CRC32C_TABLE[(crc ^ *p++) & 0xFF] ^ (crc >> 8);');
  L.push('  return ~crc;');
  L.push('}');
  L.push('');

  // ---- little-endian writer / bounded reader ---------------------------------------
  L.push('struct ZhWriter {');
  L.push('  std::vector<uint8_t>& out;');
  L.push('  explicit ZhWriter(std::vector<uint8_t>& o) : out(o) {}');
  L.push('  void u8(uint8_t v)   { out.push_back(v); }');
  L.push('  void u16(uint16_t v) { u8(v & 0xFF); u8(uint8_t(v >> 8)); }');
  L.push('  void u32(uint32_t v) { u16(uint16_t(v & 0xFFFF)); u16(uint16_t(v >> 16)); }');
  L.push('  void u64(uint64_t v) { u32(uint32_t(v & 0xFFFFFFFF)); u32(uint32_t(v >> 32)); }');
  L.push('  void i8(int8_t v)    { u8(uint8_t(v)); }');
  L.push('  void i16(int16_t v)  { u16(uint16_t(v)); }');
  L.push('  void i32(int32_t v)  { u32(uint32_t(v)); }');
  L.push('  void i64(int64_t v)  { u64(uint64_t(v)); }');
  L.push('  void fx16(int32_t v) { i32(v); }  // Q16.16');
  L.push('  void fx32(int64_t v) { i64(v); }  // Q32.32');
  L.push('};');
  L.push('');
  L.push('struct ZhReader {');
  L.push('  const uint8_t* p; size_t n; size_t pos = 0;');
  L.push('  ZhReader(const uint8_t* ptr, size_t len) : p(ptr), n(len) {}');
  L.push('  bool take8(uint8_t& v)  { if (pos + 1 > n) return false; v = p[pos++]; return true; }');
  L.push('  bool take16(uint16_t& v) { uint8_t a, b; if (!take8(a) || !take8(b)) return false; v = uint16_t(a) | (uint16_t(b) << 8); return true; }');
  L.push('  bool take32(uint32_t& v) { uint16_t a, b; if (!take16(a) || !take16(b)) return false; v = uint32_t(a) | (uint32_t(b) << 16); return true; }');
  L.push('  bool take64(uint64_t& v) { uint32_t a, b; if (!take32(a) || !take32(b)) return false; v = uint64_t(a) | (uint64_t(b) << 32); return true; }');
  L.push('  bool skip(size_t k) { if (pos + k > n) return false; pos += k; return true; }');
  L.push('};');
  L.push('');

  // ---- structs --------------------------------------------------------------------
  const emitStruct = (s: StructIR) => {
    L.push(`// ${s.name}: ${s.size} bytes (spec/commands.zidl)`);
    L.push(`struct ${structName(s.name)} {`);
    for (const f of s.fields) L.push(declField(f, s.name));
    L.push('};');
    for (const f of s.fields) {
      const base = f.count > 1 ? `${f.name}[0]` : f.name;
      L.push(`static_assert(offsetof(${structName(s.name)}, ${base}) == ${f.offset}, "layout drift: ${s.name}.${f.name}");`);
    }
    L.push(`static_assert(sizeof(${structName(s.name)}) == ${s.size}, "layout drift: ${s.name} size");`);
    L.push('');
  };
  for (const s of ir.structs.values()) emitStruct(s);

  // ---- command header + payload structs + record structs ---------------------------
  L.push('// 16-byte command record header (capture_format.md 3.1)');
  L.push('struct ZhCmdHeader {');
  L.push('  uint16_t opcode;');
  L.push('  uint16_t record_bytes;');
  L.push('  uint32_t source_id;');
  L.push('  uint32_t flags;      // no defined bits in v1 -> must be 0');
  L.push('  uint32_t reserved0;  // must be 0');
  L.push('};');
  L.push('static_assert(sizeof(ZhCmdHeader) == 16, "command header must be 16 bytes");');
  for (const [nm, off] of [['opcode', 0], ['record_bytes', 2], ['source_id', 4], ['flags', 8], ['reserved0', 12]] as const) {
    L.push(`static_assert(offsetof(ZhCmdHeader, ${nm}) == ${off}, "");`);
  }
  L.push('');

  for (const c of ir.commands) {
    L.push(`// ${c.name} ${c.opcodeHex}: ${c.recordBytes}-byte record (${c.implemented ? 'implemented' : 'reserved'})`);
    if (c.fields.length > 0) {
      L.push(`struct ${payloadName(c)} {`);
      for (const f of c.fields) L.push(declField(f, c.name));
      L.push('};');
      for (const f of c.fields) {
        const base = f.count > 1 ? `${f.name}[0]` : f.name;
        L.push(`static_assert(offsetof(${payloadName(c)}, ${base}) == ${f.offset}, "layout drift: ${c.name}.${f.name}");`);
      }
      L.push(`static_assert(sizeof(${payloadName(c)}) == ${c.recordBytes - 16}, "layout drift: ${c.name} payload");`);
      L.push('');
    }
    L.push(`struct ${recordName(c)} {`);
    L.push('  ZhCmdHeader hdr;');
    if (c.fields.length > 0) L.push(`  ${payloadName(c)} payload;`);
    L.push('};');
    L.push(`static_assert(sizeof(${recordName(c)}) == ${c.recordBytes}, "layout drift: ${c.name} record");`);
    L.push('');
  }

  // ---- struct sample functions (shared, offset-free: same bytes everywhere) ---------
  for (const name of usedStructs) {
    const s = ir.structs.get(name)!;
    const sample = structSample(ir, name);
    L.push(`inline ${structName(name)} zhao_sample_${name}() {`);
    L.push(`  ${structName(name)} v{};`);
    let li = 0;
    for (const f of s.fields) {
      if (f.kind === 'pad') continue;
      if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  v.${f.name}${f.count > 1 ? `[${k}]` : ''} = zhao_sample_${f.type}();`);
        }
        li += f.leaves.length;
        continue;
      }
      for (let k = 0; k < f.count; k++) {
        const leaf = sample.leaves[li]!;
        const val = cppValue(leaf.value, primOf(f));
        L.push(`  v.${f.name}${f.count > 1 ? `[${k}]` : ''} = ${f.kind === 'enum' ? `static_cast<${f.type}>(${val})` : val};`);
        li++;
      }
    }
    L.push('  return v;');
    L.push('}');
    L.push('');
  }

  // ---- command sample functions ------------------------------------------------------
  for (const c of ir.commands) {
    const sample = sampleCommand(ir, c);
    L.push(`inline ${recordName(c)} zhao_sample_${c.snake}() {`);
    L.push(`  ${recordName(c)} r{};`);
    L.push(`  r.hdr.opcode       = ZHAO_OP_${upperSnake(c.name)};`);
    L.push('  r.hdr.record_bytes = ' + `${c.recordBytes};`);
    L.push(`  r.hdr.source_id    = ${sample.header.sourceId}u; // kind 5, module 1, index ${c.index}`);
    L.push('  r.hdr.flags        = 0u;');
    L.push('  r.hdr.reserved0    = 0u;');
    let li = 0;
    for (const f of c.fields) {
      if (f.kind === 'pad') {
        continue; // pads produce no sample leaves (sample.ts skips them)
      }
      if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  r.payload.${f.name}${f.count > 1 ? `[${k}]` : ''} = zhao_sample_${f.type}();`);
        }
        li += f.leaves.length;
        continue;
      }
      for (let k = 0; k < f.count; k++) {
        const leaf = sample.leaves[li]!;
        const val = cppValue(leaf.value, primOf(f));
        L.push(`  r.payload.${f.name}${f.count > 1 ? `[${k}]` : ''} = ${f.kind === 'enum' ? `static_cast<${f.type}>(${val})` : val};`);
        li++;
      }
    }
    L.push('  return r;');
    L.push('}');
    L.push('');
  }

  // ---- pack helpers: structs first, then commands -------------------------------------
  for (const name of usedStructs) {
    const s = ir.structs.get(name)!;
    L.push(`inline void zhao_pack_${name}(const ${structName(name)}& v, ZhWriter& w) {`);
    for (const f of s.fields) {
      if (f.kind === 'pad') {
        L.push(`  for (int i = 0; i < ${f.count}; ++i) w.u8(v.${f.name}${f.count > 1 ? '[i]' : ''});`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  zhao_pack_${f.type}(v.${f.name}${f.count > 1 ? `[${k}]` : ''}, w);`);
        }
      } else if (f.count > 1) {
        L.push(`  for (int i = 0; i < ${f.count}; ++i) { ${writeScalar(primOf(f), `v.${f.name}[i]`)} }`);
      } else {
        L.push(`  ${writeScalar(primOf(f), `v.${f.name}`)}`);
      }
    }
    L.push('}');
    L.push('');
  }

  for (const c of ir.commands) {
    L.push(`inline void zhao_pack_${c.snake}(const ${recordName(c)}& r, std::vector<uint8_t>& out) {`);
    L.push('  ZhWriter w(out);');
    L.push('  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);');
    L.push('  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);');
    for (const f of c.fields) {
      if (f.kind === 'pad') {
        L.push(`  for (int i = 0; i < ${f.count}; ++i) w.u8(r.payload.${f.name}${f.count > 1 ? '[i]' : ''});`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  zhao_pack_${f.type}(r.payload.${f.name}${f.count > 1 ? `[${k}]` : ''}, w);`);
        }
      } else if (f.count > 1) {
        L.push(`  for (int i = 0; i < ${f.count}; ++i) { ${writeScalar(primOf(f), `r.payload.${f.name}[i]`)} }`);
      } else {
        L.push(`  ${writeScalar(primOf(f), `r.payload.${f.name}`)}`);
      }
    }
    L.push('}');
    L.push('');
  }

  // ---- unpack helpers ------------------------------------------------------------------
  for (const name of usedStructs) {
    const s = ir.structs.get(name)!;
    L.push(`inline bool zhao_unpack_${name}(ZhReader& r, ${structName(name)}& out) {`);
    L.push('  out = {};');
    for (const f of s.fields) {
      if (f.kind === 'pad') {
        L.push(`  if (!r.skip(${f.count})) return false;`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  if (!zhao_unpack_${f.type}(r, out.${f.name}${f.count > 1 ? `[${k}]` : ''})) return false;`);
        }
      } else {
        for (let k = 0; k < f.count; k++) {
          L.push(`  ${readScalar(primOf(f), `out.${f.name}${f.count > 1 ? `[${k}]` : ''}`, f.kind === 'enum' ? f.type : undefined)}`);
        }
      }
    }
    L.push('  return true;');
    L.push('}');
    L.push('');
  }

  for (const c of ir.commands) {
    L.push(`inline bool zhao_unpack_${c.snake}(ZhReader& r, ${recordName(c)}& out) {`);
    L.push('  out = {};');
    L.push('  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||');
    L.push('      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||');
    L.push('      !r.take32(out.hdr.reserved0)) return false;');
    for (const f of c.fields) {
      if (f.kind === 'pad') {
        L.push(`  if (!r.skip(${f.count})) return false;`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  if (!zhao_unpack_${f.type}(r, out.payload.${f.name}${f.count > 1 ? `[${k}]` : ''})) return false;`);
        }
      } else {
        for (let k = 0; k < f.count; k++) {
          L.push(`  ${readScalar(primOf(f), `out.payload.${f.name}${f.count > 1 ? `[${k}]` : ''}`, f.kind === 'enum' ? f.type : undefined)}`);
        }
      }
    }
    L.push('  return true;');
    L.push('}');
    L.push('');
  }

  // ---- command table ---------------------------------------------------------------------
  L.push('struct ZhCommandInfo {');
  L.push('  const char* name;');
  L.push('  uint16_t opcode;');
  L.push('  uint16_t record_bytes;');
  L.push('  bool implemented;');
  L.push('  const uint16_t* pad_offsets;  // payload-relative must-be-zero bytes');
  L.push('  uint16_t pad_count;');
  L.push('};');
  for (const c of ir.commands) {
    const pads: number[] = [];
    for (const f of c.fields) {
      if (f.kind === 'pad') for (let k = 0; k < f.count; k++) pads.push(f.offset + k);
    }
    if (pads.length > 0) {
      L.push(`constexpr uint16_t ZHAO_PADS_${upperSnake(c.name)}[] = {${pads.join(', ')}};`);
    }
  }
  L.push('constexpr ZhCommandInfo ZHAO_COMMAND_TABLE[] = {');
  for (const c of ir.commands) {
    const pads: number[] = [];
    for (const f of c.fields) {
      if (f.kind === 'pad') for (let k = 0; k < f.count; k++) pads.push(f.offset + k);
    }
    const padRef = pads.length > 0 ? `ZHAO_PADS_${upperSnake(c.name)}, ${pads.length}` : 'nullptr, 0';
    L.push(`  {"${c.name}", ${c.opcodeHex}, ${c.recordBytes}, ${c.implemented ? 'true' : 'false'}, ${padRef}},`);
  }
  L.push('};');
  L.push(`constexpr size_t ZHAO_COMMAND_COUNT = ${ir.commands.length};`);
  L.push(`constexpr uint16_t ZHAO_MAX_RECORD_BYTES = ${Math.max(...ir.commands.map((c) => c.recordBytes))};`);
  L.push('inline const ZhCommandInfo* zhao_command_info(uint16_t opcode) {');
  L.push('  for (const auto& e : ZHAO_COMMAND_TABLE) if (e.opcode == opcode) return &e;');
  L.push('  return nullptr;');
  L.push('}');
  L.push('');

  // ---- enum range check (capture_format.md 3.2 step 7, active since ABI v2) ------
  // p points at the record PAYLOAD (record start + 16). True iff every enum
  // field carries a declared member value; false => ZH_ABI_BAD_VALUE.
  L.push('inline bool zhao_enum_value_ok(uint16_t opcode, const uint8_t* p) {');
  L.push('  switch (opcode) {');
  for (const c of ir.commands) {
    const checks = c.fields.filter((f) => f.kind === 'enum');
    if (checks.length === 0) continue;
    L.push(`    case ZHAO_OP_${upperSnake(c.name)}: {`);
    for (let i = 0; i < checks.length; i++) {
      const f = checks[i]!;
      const size = f.size / f.count;
      const v = size === 1 ? `uint32_t(p[${f.offset}])`
        : size === 2 ? `(uint32_t(p[${f.offset}]) | (uint32_t(p[${f.offset + 1}]) << 8))`
          : `(uint32_t(p[${f.offset}]) | (uint32_t(p[${f.offset + 1}]) << 8) | (uint32_t(p[${f.offset + 2}]) << 16) | (uint32_t(p[${f.offset + 3}]) << 24))`;
      const entries = ir.enums.find((e) => e.name === f.type)!.entries;
      const members = entries.map((x) => `v${i} == ${x.value}u`).join(' || ');
      L.push(`      const uint32_t v${i} = ${v};  // ${f.name}: ${f.type}`);
      L.push(`      if (!(${members})) return false;`);
    }
    L.push('      return true;');
    L.push('    }');
  }
  L.push('    default: return true;');
  L.push('  }');
  L.push('}');
  L.push('');

  // ---- .zcap ABI_INFO identity ------------------------------------------------------------
  L.push('// .zcap ABI_INFO identity (capture_format.md 4.2)');
  L.push('inline constexpr const char* ZHAO_GENERATOR_NAME = "zhaozhou-abi-gen";');
  L.push(`inline constexpr uint8_t ZHAO_GENERATOR_SHA256[32] = {${hexBytes(identitySha256)}};`);
  L.push(`inline constexpr uint8_t ZHAO_ZIDL_SHA256[32] = {${hexBytes(zidlSha256)}};`);
  L.push('inline constexpr uint32_t ZHAO_ZCAP_SCHEMA_VERSION = 1;');
  L.push('');

  L.push('}  // namespace zhao_abi');
  L.push('');  return L.join('\n');
}
