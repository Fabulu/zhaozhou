// emit_ts.ts — compiler/src/generated/abi.ts emitter (P5 1.5). Plain ES
// module, no imports: const tables, little-endian writer, CRC-32C table
// literal, SHA-256 (FIPS 180-4), sample records, packers, command table.
// The consumer-side frame/.zcap mirrors (frame.ts/zcap.ts) import from here.

import { CRC32C_TABLE } from './crc32c.js';
import { sampleCommand, structSample } from './sample.js';
import { CommandIR, FieldIR, LayoutIR, upperSnake } from './types.js';

const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);
const structIface = (s: string) => `Zh${cap(s)}`;

const SHA256_K: readonly number[] = [
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
];

function hexBytes(hex: string): string {
  const out: string[] = [];
  for (let i = 0; i < 64; i += 2) out.push(`0x${hex.slice(i, i + 2).toUpperCase()}`);
  return out.join(', ');
}

export function emitTs(ir: LayoutIR, identitySha256: string, zidlSha256: string): string {
  const L: string[] = [];
  // wire prim of a field: handles are u32; enums ride their BACKING prim
  const primOf = (f: FieldIR) => (f.kind === 'handle' ? 'u32'
    : f.kind === 'enum' ? f.leaves[0]!.prim
      : f.type === 'angle16' ? 'u16' : f.type);
  const arrSuffix = (f: FieldIR) => (f.count > 1 ? '[]' : '');
  const tsFieldType = (f: FieldIR): string => {
    if (f.kind === 'struct') return `${structIface(f.type)}${arrSuffix(f)}`;
    if (f.kind === 'handle') return `number${arrSuffix(f)}; // handle32 {index:24, generation:8}`;
    const note = f.type === 'fx16' ? 'number; // fx16 = Q16.16 in int32 (qformats.md)'
      : f.type === 'fx32' ? 'number; // fx32 = Q32.32 in int64'
        : null;
    return note ? `${note}${arrSuffix(f) ? '' : ''}` : undefined as never;
  };

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
  L.push('');
  L.push('// ---------------------------------------------------------------- abi ---');
  L.push('');
  L.push(`export const ZHAO_ABI_VERSION = ${ir.abi.version} as const;`);
  L.push(`export const ZHAO_COMMAND_ALIGNMENT = ${ir.abi.commandAlignment} as const;`);
  for (const c of ir.consts) {
    L.push(`export const ${c.name} = ${c.value} as const;`);
  }
  L.push('');
  L.push('// error codes — shared verbatim across C++/TS/SV (zhao_abi.h / zhao_abi_pkg.sv)');
  const errEnum = ir.enums.find((e) => e.name === 'zhao_abi_error');
  if (errEnum) {
    for (const e of errEnum.entries) L.push(`export const ${e.name} = ${e.value} as const;`);
    L.push('export const ZHAO_ERROR_NAMES: Record<number, string> = {');
    for (const e of errEnum.entries) L.push(`  [${e.value}]: '${e.name}',`);
    L.push('};');
    L.push('');
  }
  // value enums (ABI v2): members + the valid-value sets the validator needs
  for (const e of ir.enums) {
    if (e.name === 'zhao_abi_error') continue;
    L.push(`// enum ${e.name}: ${e.type} on the wire (capture_format.md 3.2 step 7)`);
    for (const entry of e.entries) L.push(`export const ${entry.name} = ${entry.value} as const;`);
    L.push(`export const ZHAO_ENUM_${upperSnake(e.name)}: readonly number[] = [${e.entries.map((x) => x.value).join(', ')}];`);
    L.push('');
  }
  L.push('// opcodes');
  for (const c of ir.commands) {
    L.push(`export const ZHAO_OP_${upperSnake(c.name)} = ${c.opcodeHex}; // ${c.recordBytes} B, ${c.implemented ? 'implemented' : 'reserved'}`);
  }
  L.push('');
  L.push('// frame packet (capture_format.md 3)');
  L.push("export const ZHAO_FRAME_MAGIC = 0x314b505a; // 'Z','P','K','1' LE");
  L.push('export const ZHAO_FRAME_HEADER_BYTES = 36;');
  L.push('export const ZHAO_FRAME_OVERHEAD = 40;');
  L.push('export const ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 0x0001;');
  L.push('export const ZHAO_COMPL_DONE = 0x01;');
  L.push('export const ZHAO_COMPL_ERR = 0x02;');
  for (const [nm, off] of [['MAGIC', 0], ['ABI_VERSION', 4], ['FLAGS', 6], ['FRAME_ID', 8],
    ['SEQUENCE', 12], ['RESOURCE_EPOCH', 16], ['DEADLINE', 20], ['COMMAND_COUNT', 24],
    ['COMMAND_BYTES', 28], ['HEADER_CRC', 32]] as const) {
    L.push(`export const ZHAO_OFF_${nm} = ${off} as const;`);
  }
  L.push('');
  L.push('// source-id scheme (capture_format.md 5)');
  L.push('export const ZHAO_SOURCE_KIND_NONE = 0;');
  L.push('export const ZHAO_SOURCE_KIND_COMMAND_SITE = 5;');
  L.push('export function zhaoSourceIdEncode(kind: number, module: number, index: number): number {');
  L.push('  return ((kind << 28) | (module << 16) | index) >>> 0;');
  L.push('}');
  L.push('export function zhaoSourceIdDecode(id: number): { kind: number; module: number; index: number } {');
  L.push('  return { kind: id >>> 28, module: (id >>> 16) & 0xfff, index: id & 0xffff };');
  L.push('}');
  L.push('');

  // ---- struct interfaces -------------------------------------------------------
  for (const s of ir.structs.values()) {
    L.push(`/** ${s.name}: ${s.size} bytes (spec/commands.zidl); pads are not modeled */`);
    L.push(`export interface ${structIface(s.name)} {`);
    for (const f of s.fields) {
      if (f.kind === 'pad') continue;
      if (f.kind === 'struct') {
        L.push(`  ${f.name}: ${structIface(f.type)}${arrSuffix(f)}; // @${f.offset}`);
      } else if (f.kind === 'handle') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // handle32, @${f.offset}`);
      } else if (f.type === 'fx16') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // fx16 (Q16.16, int32), @${f.offset}`);
      } else if (f.type === 'fx32') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // fx32 (Q32.32, int64), @${f.offset}`);
      } else if (f.type === 'angle16') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // angle16 (U 0.0.16 turns, u16), @${f.offset}`);
      } else if (f.kind === 'enum') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // ${f.type} (${f.leaves[0]!.prim}), @${f.offset}`);
      } else {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // ${f.type}, @${f.offset}`);
      }
    }
    L.push('}');
    L.push('');
  }

  // ---- record interfaces --------------------------------------------------------
  L.push('/** 16-byte command record header (capture_format.md 3.1) */');
  L.push('export interface ZhCmdHeader {');
  L.push('  opcode: number;');
  L.push('  recordBytes: number;');
  L.push('  sourceId: number;');
  L.push('  flags: number; // no defined bits in v1 -> must be 0');
  L.push('}');
  L.push('');
  for (const c of ir.commands) {
    L.push(`/** ${c.name} ${c.opcodeHex}: ${c.recordBytes}-byte record (${c.implemented ? 'implemented' : 'reserved'}) */`);
    L.push(`export interface ZhRecord${c.name} {`);
    L.push('  hdr: ZhCmdHeader;');
    for (const f of c.fields) {
      if (f.kind === 'pad') continue;
      if (f.kind === 'struct') {
        L.push(`  ${f.name}: ${structIface(f.type)}${arrSuffix(f)}; // @${f.offset}`);
      } else if (f.kind === 'handle') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // handle32, @${f.offset}`);
      } else if (f.type === 'fx16') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // fx16 (Q16.16, int32), @${f.offset}`);
      } else if (f.type === 'fx32') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // fx32 (Q32.32, int64), @${f.offset}`);
      } else if (f.type === 'angle16') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // angle16 (U 0.0.16 turns, u16), @${f.offset}`);
      } else if (f.kind === 'enum') {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // ${f.type} (${f.leaves[0]!.prim}), @${f.offset}`);
      } else {
        L.push(`  ${f.name}: number${arrSuffix(f)}; // ${f.type}, @${f.offset}`);
      }
    }
    L.push('}');
    L.push('');
  }

  // ---- command table --------------------------------------------------------------
  L.push('export interface ZhCommandInfo {');
  L.push('  name: string;');
  L.push('  opcode: number;');
  L.push('  recordBytes: number;');
  L.push('  implemented: boolean;');
  L.push('  /** payload-relative byte offsets that must be zero (pads) */');
  L.push('  padOffsets: readonly number[];');
  L.push('  /** enum range checks (capture_format.md 3.2 step 7, ABI v2) */');
  L.push('  enumChecks: readonly { readonly offset: number; readonly size: number; readonly values: readonly number[] }[];');
  L.push('}');
  L.push('export const ZHAO_COMMAND_TABLE: readonly ZhCommandInfo[] = [');
  for (const c of ir.commands) {
    const pads: number[] = [];
    for (const f of c.fields) {
      if (f.kind === 'pad') for (let k = 0; k < f.count; k++) pads.push(f.offset + k);
    }
    const enums = c.fields
      .filter((f) => f.kind === 'enum')
      .map((f) => `{ offset: ${f.offset}, size: ${f.size / f.count}, values: [${ir.enums.find((e) => e.name === f.type)!.entries.map((x) => x.value).join(', ')}] }`);
    L.push(`  { name: '${c.name}', opcode: ${c.opcodeHex}, recordBytes: ${c.recordBytes}, implemented: ${c.implemented}, padOffsets: [${pads.join(', ')}], enumChecks: [${enums.join(', ')}] },`);
  }
  L.push('];');
  L.push(`export const ZHAO_COMMAND_COUNT = ${ir.commands.length} as const;`);
  L.push(`export const ZHAO_MAX_RECORD_BYTES = ${Math.max(...ir.commands.map((c) => c.recordBytes))} as const;`);
  L.push('export function zhaoCommandInfo(opcode: number): ZhCommandInfo | undefined {');
  L.push('  return ZHAO_COMMAND_TABLE.find((c) => c.opcode === opcode);');
  L.push('}');
  L.push('');

  // ---- CRC-32C ----------------------------------------------------------------------
  L.push('// CRC-32C (Castagnoli): poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF');
  L.push('export const ZHAO_CRC32C_TABLE: readonly number[] = [');
  for (let i = 0; i < 256; i += 8) {
    L.push(`  ${[0, 1, 2, 3, 4, 5, 6, 7].map((k) => `0x${(CRC32C_TABLE[i + k]!).toString(16).padStart(8, '0')}`).join(', ')},`);
  }
  L.push('];');
  L.push('/** running form: crc=0 fresh, feed the previous return to continue (capture_format.md 2) */');
  L.push('export function crc32c(crc: number, buf: Uint8Array, off = 0, len = buf.length - off): number {');
  L.push('  let c = (crc ^ 0xffffffff) >>> 0;');
  L.push('  for (let i = 0; i < len; i++) {');
  L.push('    c = ((ZHAO_CRC32C_TABLE[(c ^ buf[off + i]!) & 0xff]! ^ (c >>> 8)) & 0xffffffff) >>> 0;');
  L.push('  }');
  L.push('  return (c ^ 0xffffffff) >>> 0;');
  L.push('}');
  L.push('');

  // ---- SHA-256 (FIPS 180-4) — identical to C++ zref_sha256.hpp ------------------------
  L.push('const SHA256_K = new Uint32Array([');
  for (let i = 0; i < 64; i += 8) {
    L.push(`  ${[0, 1, 2, 3, 4, 5, 6, 7].map((k) => `0x${SHA256_K[i + k]!.toString(16)}`).join(', ')},`);
  }
  L.push(']);');
  L.push('function rotr(x: number, n: number): number {');
  L.push('  return ((x >>> n) | (x << (32 - n))) >>> 0;');
  L.push('}');
  L.push('/** SHA-256 (FIPS 180-4). Byte-identical to zref_sha256.hpp (locked by goldens). */');
  L.push('export function sha256(data: Uint8Array): Uint8Array {');
  L.push('  const h = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,');
  L.push('    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);');
  L.push('  const bitLen = data.length * 8;');
  L.push('  const padded = new Uint8Array((((data.length + 8) >> 6) + 1) << 6);');
  L.push('  padded.set(data);');
  L.push('  padded[data.length] = 0x80;');
  L.push('  const dv = new DataView(padded.buffer);');
  L.push('  dv.setUint32(padded.length - 4, bitLen >>> 0, false);');
  L.push('  dv.setUint32(padded.length - 8, Math.floor(bitLen / 0x100000000), false);');
  L.push('  const w = new Uint32Array(64);');
  L.push('  for (let chunk = 0; chunk < padded.length; chunk += 64) {');
  L.push('    for (let i = 0; i < 16; i++) w[i] = dv.getUint32(chunk + i * 4, false);');
  L.push('    for (let i = 16; i < 64; i++) {');
  L.push('      const s0 = rotr(w[i - 15]!, 7) ^ rotr(w[i - 15]!, 18) ^ (w[i - 15]! >>> 3);');
  L.push('      const s1 = rotr(w[i - 2]!, 17) ^ rotr(w[i - 2]!, 19) ^ (w[i - 2]! >>> 10);');
  L.push('      w[i] = (w[i - 16]! + s0 + w[i - 7]! + s1) >>> 0;');
  L.push('    }');
  L.push('    let [a, b, c, d, e, f, g, hh] = [h[0]!, h[1]!, h[2]!, h[3]!, h[4]!, h[5]!, h[6]!, h[7]!];');
  L.push('    for (let i = 0; i < 64; i++) {');
  L.push('      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);');
  L.push('      const ch = (e & f) ^ (~e & g);');
  L.push('      const t1 = (hh + S1 + ch + SHA256_K[i]! + w[i]!) >>> 0;');
  L.push('      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);');
  L.push('      const maj = (a & b) ^ (a & c) ^ (b & c);');
  L.push('      const t2 = (S0 + maj) >>> 0;');
  L.push('      hh = g; g = f; f = e; e = (d + t1) >>> 0; d = c; c = b; b = a; a = (t1 + t2) >>> 0;');
  L.push('    }');
  L.push('    h[0] = (h[0]! + a) >>> 0; h[1] = (h[1]! + b) >>> 0; h[2] = (h[2]! + c) >>> 0; h[3] = (h[3]! + d) >>> 0;');
  L.push('    h[4] = (h[4]! + e) >>> 0; h[5] = (h[5]! + f) >>> 0; h[6] = (h[6]! + g) >>> 0; h[7] = (h[7]! + hh) >>> 0;');
  L.push('  }');
  L.push('  const out = new Uint8Array(32);');
  L.push('  const odv = new DataView(out.buffer);');
  L.push('  for (let i = 0; i < 8; i++) odv.setUint32(i * 4, h[i]!, false);');
  L.push('  return out;');
  L.push('}');
  L.push('');

  // ---- little-endian writer ---------------------------------------------------------
  L.push('/** little-endian byte writer */');
  L.push('export class ZhByteWriter {');
  L.push('  readonly bytes: number[] = [];');
  L.push('  u8(v: number): void { this.bytes.push(v & 0xff); }');
  L.push('  u16(v: number): void { this.u8(v); this.u8(v >>> 8); }');
  L.push('  u32(v: number): void { this.u16(v & 0xffff); this.u16((v >>> 16) & 0xffff); }');
  L.push('  i8(v: number): void { this.u8(v); }');
  L.push('  i16(v: number): void { this.u16(v & 0xffff); }');
  L.push('  i32(v: number): void { this.u32(v >>> 0); }');
  L.push('  fx16(v: number): void { this.i32(v); } // Q16.16');
  L.push('  zeros(n: number): void { for (let i = 0; i < n; i++) this.bytes.push(0); }');
  L.push('  raw(bytes: Uint8Array | readonly number[]): void { for (const b of bytes) this.bytes.push(b & 0xff); }');
  L.push('  toUint8Array(): Uint8Array { return Uint8Array.from(this.bytes); }');
  L.push('}');
  L.push('');

  // ---- struct samples (shared; same bytes in every embedding command) ------------------
  for (const name of usedStructs) {
    const s = ir.structs.get(name)!;
    const sample = structSample(ir, name);
    L.push(`export function zhaoSample${cap(name)}(): ${structIface(name)} {`);
    L.push('  return {');
    let li = 0;
    for (const f of s.fields) {
      if (f.kind === 'pad') {
        continue; // pads produce no sample leaves (sample.ts skips them)
      }
      if (f.kind === 'struct') {
        const items = Array.from({ length: f.count }, () => `zhaoSample${cap(f.type)}()`);
        L.push(`    ${f.name}: ${f.count > 1 ? `[${items.join(', ')}]` : items[0]!},`);
        li += f.leaves.length;
        continue;
      }
      const vals = Array.from({ length: f.count }, () => {
        const leaf = sample.leaves[li]!;
        li++;
        return `${leaf.value}`;
      });
      L.push(`    ${f.name}: ${f.count > 1 ? `[${vals.join(', ')}]` : vals[0]!},`);
    }
    L.push('  };');
    L.push('}');
    L.push('');
  }

  // ---- command samples ------------------------------------------------------------------
  for (const c of ir.commands) {
    const sample = sampleCommand(ir, c);
    L.push(`export function zhaoSample${c.name}(): ZhRecord${c.name} {`);
    L.push('  return {');
    L.push('    hdr: {');
    L.push(`      opcode: ZHAO_OP_${upperSnake(c.name)},`);
    L.push(`      recordBytes: ${c.recordBytes},`);
    L.push(`      sourceId: ${sample.header.sourceId}, // kind 5, module 1, index ${c.index}`);
    L.push('      flags: 0,');
    L.push('    },');
    let li = 0;
    for (const f of c.fields) {
      if (f.kind === 'pad') {
        continue; // pads produce no sample leaves (sample.ts skips them)
      }
      if (f.kind === 'struct') {
        const items = Array.from({ length: f.count }, () => `zhaoSample${cap(f.type)}()`);
        L.push(`    ${f.name}: ${f.count > 1 ? `[${items.join(', ')}]` : items[0]!},`);
        li += f.leaves.length;
        continue;
      }
      const vals = Array.from({ length: f.count }, () => {
        const leaf = sample.leaves[li]!;
        li++;
        return `${leaf.value}`;
      });
      L.push(`    ${f.name}: ${f.count > 1 ? `[${vals.join(', ')}]` : vals[0]!},`);
    }
    L.push('  };');
    L.push('}');
    L.push('');
  }

  // ---- pack helpers (structs first) -------------------------------------------------------
  for (const name of usedStructs) {
    const s = ir.structs.get(name)!;
    L.push(`export function zhaoPack${cap(name)}(v: ${structIface(name)}, w: ZhByteWriter): void {`);
    for (const f of s.fields) {
      if (f.kind === 'pad') {
        L.push(`  w.zeros(${f.count}); // ${f.name}`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  zhaoPack${cap(f.type)}(v.${f.name}${f.count > 1 ? `[${k}]!` : ''}, w);`);
        }
      } else if (f.count > 1) {
        L.push(`  for (let i = 0; i < ${f.count}; i++) w.${primOf(f)}(v.${f.name}[i]!);`);
      } else {
        L.push(`  w.${primOf(f)}(v.${f.name});`);
      }
    }
    L.push('}');
    L.push('');
  }

  for (const c of ir.commands) {
    L.push(`export function zhaoPack${c.name}(r: ZhRecord${c.name}, w: ZhByteWriter): void {`);
    L.push('  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);');
    L.push('  w.u32(r.hdr.flags); w.zeros(4); // reserved0');
    for (const f of c.fields) {
      if (f.kind === 'pad') {
        L.push(`  w.zeros(${f.count}); // ${f.name}`);
      } else if (f.kind === 'struct') {
        for (let k = 0; k < f.count; k++) {
          L.push(`  zhaoPack${cap(f.type)}(r.${f.name}${f.count > 1 ? `[${k}]!` : ''}, w);`);
        }
      } else if (f.count > 1) {
        L.push(`  for (let i = 0; i < ${f.count}; i++) w.${primOf(f)}(r.${f.name}[i]!);`);
      } else {
        L.push(`  w.${primOf(f)}(r.${f.name});`);
      }
    }
    L.push('}');
    L.push('');
  }

  // ---- identity (.zcap ABI_INFO) -------------------------------------------------------------
  L.push('// .zcap ABI_INFO identity (capture_format.md 4.2)');
  L.push("export const ZHAO_GENERATOR_NAME = 'zhaozhou-abi-gen';");
  L.push(`export const ZHAO_GENERATOR_SHA256: readonly number[] = [${hexBytes(identitySha256)}];`);
  L.push(`export const ZHAO_ZIDL_SHA256: readonly number[] = [${hexBytes(zidlSha256)}];`);
  L.push('export const ZHAO_ZCAP_SCHEMA_VERSION = 1;');
  L.push('');

  void tsFieldType;
  return L.join('\n');
}
