// serialize.ts — deterministic .zprog writer + full-validating reader.
//
// Spec: spec/form/field-ir.md (FIELD_IR_VERSION 1)
//   §5    .zprog layout (28-byte header, code|tables|map body)
//   §5.4  program hash = CRC-32C(code‖tables) + instr_count (u32 wrap)
//   §5.5  body CRC = CRC-32C over the file with the CRC field zeroed
//   §4    validator V1..V12 (the reader runs the FULL rule set — never trust)
//
// Determinism: no timestamps, no host paths, fixed widths (qformats.md §11
// discipline). Two serializations of the same program are byte-identical
// (asserted in compiler/tests/field_ir.test.ts).

import { crc32c } from '../generated/abi.js';
import {
  FieldProgram, FieldTable, IoLane, OpName, OP_INFO, PROFILE_NAMES,
  PROFILE_CEILING, GLOBAL_CEILING, LANE_TYPE_CODE, TABLE_KIND_CODE,
  REG_COUNT, FIELD_IR_VERSION, opFromCode,
} from './types.js';

const OP_BY_CODE = new Map<number, OpName>(
  (Object.keys(OP_INFO) as OpName[]).map((n) => [OP_INFO[n]!.code, n]),
);

// §1.1 word packing: word = op | dst<<8 | srcA<<14 | srcB<<20 | srcC<<26 | imm<<32
function packWord(op: number, dst: number, a: number, b: number, c: number,
                  imm: number, w: ByteWriter): void {
  w.u8(op);
  w.u8((dst | (a << 6)) & 0xff);            // dst @8..13, srcA low 2 @14..15
  w.u8(((a >>> 2) | ((b & 0x0f) << 4)) & 0xff);  // srcA high 4, srcB low 4
  w.u8(((b >>> 4) | (c << 2)) & 0xff);      // srcB high 2, srcC @26..31
  w.u32(imm >>> 0);
}

function unpackWord(r: ByteReader): DecodedInstr {
  const b0 = r.u8(), b1 = r.u8(), b2 = r.u8(), b3 = r.u8();
  return {
    op: b0,
    dst: b1 & 0x3f,
    a: ((b1 >>> 6) | ((b2 & 0x0f) << 2)) & 0x3f,
    b: ((b2 >>> 4) | ((b3 & 0x03) << 4)) & 0x3f,
    c: (b3 >>> 2) & 0x3f,
    imm: r.u32(),
  };
}

export const ZPROG_MAGIC = 0x5049465a;  // 'Z','F','I','P' LE
export const ZPROG_HEADER_BYTES = 28;

// ---------------------------------------------------------------------------
// little-endian writer/reader
// ---------------------------------------------------------------------------

export class ByteWriter {
  private buf: number[] = [];
  u8(v: number): void { this.buf.push(v & 0xff); }
  u16(v: number): void { this.u8(v); this.u8(v >>> 8); }
  u32(v: number): void { this.u16(v & 0xffff); this.u16((v >>> 16) & 0xffff); }
  i32(v: number): void { this.u32(v >>> 0); }
  bytes(b: ArrayLike<number>): void { for (let i = 0; i < b.length; i++) this.u8(b[i]!); }
  get length(): number { return this.buf.length; }
  toUint8Array(): Uint8Array { return Uint8Array.from(this.buf); }
}

export class ByteReader {
  pos = 0;
  constructor(public readonly buf: Uint8Array) {}
  get remaining(): number { return this.buf.length - this.pos; }
  u8(): number { return this.buf[this.pos++]!; }
  u16(): number { return this.u8() | (this.u8() << 8); }
  u32(): number { return (this.u16() | (this.u16() << 16)) >>> 0; }
  i32(): number { return this.u32() | 0; }
  u64(): number { return this.u32() + this.u32() * 4294967296; }
  bytes(n: number): Uint8Array { return this.buf.subarray(this.pos, (this.pos += n)); }
}

// ---------------------------------------------------------------------------
// program hash (field-ir.md §5.4)
// ---------------------------------------------------------------------------

export function programHash(codeBytes: Uint8Array, tableBytes: Uint8Array,
                            instrCount: number): number {
  return ((crc32c(crc32c(0, codeBytes), tableBytes) + instrCount) >>> 0);
}

/** Hash straight out of a serialized .zprog (code + tables sliced per header). */
export function programHashOfBytes(file: Uint8Array): number {
  const instrCount = file[12]! | (file[13]! << 8);
  const tableBytes = file[16]! | (file[17]! << 8);
  const codeStart = ZPROG_HEADER_BYTES;
  const code = file.subarray(codeStart, codeStart + 8 * instrCount);
  const tables = file.subarray(codeStart + 8 * instrCount,
                               codeStart + 8 * instrCount + tableBytes);
  return programHash(code, tables, instrCount);
}

// ---------------------------------------------------------------------------
// table section (shared by writer and reader)
// ---------------------------------------------------------------------------

function tableDy(table: FieldTable): number[] {
  if (table.dy.length > 0) return table.dy;
  throw new Error('table dy not precomputed (builder bug)');
}

function writeTables(w: ByteWriter, tables: FieldTable[]): void {
  for (const t of tables) {
    w.u8(TABLE_KIND_CODE[t.kind]);
    w.u8(0);
    w.u16(t.points.length);
    for (let i = 0; i < t.points.length; i++) {
      w.i32(t.points[i]!.x);
      w.i32(t.points[i]!.y);
      w.i32(tableDy(t)[i]!);
    }
  }
}

// ---------------------------------------------------------------------------
// serialize (field-ir.md §5)
// ---------------------------------------------------------------------------

export function serializeProgram(prog: FieldProgram): Uint8Array {
  const codeW = new ByteWriter();
  for (const ins of prog.code) {
    packWord(OP_INFO[ins.op].code, ins.dst & 0x3f, ins.a & 0x3f,
             ins.b & 0x3f, ins.c & 0x3f, ins.imm | 0, codeW);
  }
  const codeBytes = codeW.toUint8Array();

  const tableW = new ByteWriter();
  writeTables(tableW, prog.tables);
  const tableBytes = tableW.toUint8Array();

  // map section: io lanes | source map | name pool
  const mapW = new ByteWriter();
  const lanes = [...prog.inputs, ...prog.outputs];
  for (let i = 0; i < lanes.length; i++) {
    const lane = lanes[i]!;
    mapW.u8(lane.reg & 0x3f);
    mapW.u8(i < prog.inputs.length ? 0 : 1);
    mapW.u8(LANE_TYPE_CODE[lane.type]);
    mapW.u8(i);                     // name_id == lane ordinal (§5.3)
    const isIn = i < prog.inputs.length;
    mapW.i32(isIn ? lane.min : 0);
    mapW.i32(isIn ? lane.max : 0);
  }
  for (const ins of prog.code) {
    mapW.u32(ins.span.sourceId >>> 0);
    mapW.u16(ins.span.line);
    mapW.u16(ins.span.col);
  }
  for (const lane of lanes) {
    for (let j = 0; j < lane.name.length; j++) mapW.u8(lane.name.charCodeAt(j));
    mapW.u8(0);                     // NUL-terminated
  }
  const mapBytes = mapW.toUint8Array();

  const hash = programHash(codeBytes, tableBytes, prog.code.length);

  const h = new ByteWriter();
  h.u32(ZPROG_MAGIC);
  h.u16(FIELD_IR_VERSION);
  h.u8(PROFILE_NAMES.indexOf(prog.profile));
  h.u8(0);                          // flags
  h.u32(prog.sourceId >>> 0);
  h.u16(prog.code.length);
  h.u8(prog.tables.length);
  h.u8(lanes.length);
  h.u16(tableBytes.length);
  h.u16(mapBytes.length);
  h.u32(hash);
  h.u32(0);                         // body_crc32c placeholder

  const file = new ByteWriter();
  file.bytes(h.toUint8Array());
  file.bytes(codeBytes);
  file.bytes(tableBytes);
  file.bytes(mapBytes);
  const out = file.toUint8Array();
  const crc = crc32c(0, out);       // field bytes are zero here (§5.5)
  new DataView(out.buffer, out.byteOffset).setUint32(24, crc, true);
  return out;
}

// ---------------------------------------------------------------------------
// decode + FULL validation (field-ir.md §4) — never trust loaded bytes
// ---------------------------------------------------------------------------

/** Instruction in decoded (serialized-form) shape; imm is the raw u32. */
export interface DecodedInstr {
  op: number; dst: number; a: number; b: number; c: number; imm: number;
}

export interface DecodedTable {
  kind: number;                     // 0 curve, 1 spline
  x: Int32Array; y: Int32Array; dy: Int32Array;
}

export interface DecodedProgram {
  profile: number;
  sourceId: number;
  instrs: DecodedInstr[];
  srcMap: { sourceId: number; line: number; col: number }[];
  tables: DecodedTable[];
  inLanes: { name: string; type: number; reg: number; min: number; max: number }[];
  outLanes: { name: string; type: number; reg: number }[];
  programHash: number;
  bytes: Uint8Array;
}

export type DecodeResult =
  | { ok: true; prog: DecodedProgram }
  | { ok: false; errors: string[] };

/**
 * Structural + CRC validation of a .zprog byte image, mirroring the C++
 * loader (zfield::decode) rule for rule (§4 V1–V3 and the byte-level halves
 * of V4–V12; the register/opcode rules run over the decoded instruction
 * stream and are shared with validateProgram below via the same rule list).
 */
export function decodeZprog(file: Uint8Array): DecodeResult {
  const errors: string[] = [];
  const r = new ByteReader(file);
  const bad = (msg: string) => { errors.push(msg); };

  if (file.length < ZPROG_HEADER_BYTES) return { ok: false, errors: ['V1: file shorter than header'] };
  if (r.u32() !== ZPROG_MAGIC) bad('V1: bad magic');
  if (r.u16() !== FIELD_IR_VERSION) bad('V1: bad version');
  const profile = r.u8();
  if (profile >= PROFILE_NAMES.length) bad('V3: profile out of range');
  const flags = r.u8();
  if (flags !== 0) bad('V3: flags nonzero');
  const sourceId = r.u32();
  const instrCount = r.u16();
  const tableCount = r.u8();
  const ioLaneCount = r.u8();
  const tableBytes = r.u16();
  const mapBytes = r.u16();
  const hashField = r.u32();
  const bodyCrc = r.u32();

  const ceiling = profile < PROFILE_NAMES.length
    ? PROFILE_CEILING[PROFILE_NAMES[profile]!]! : 0;
  if (instrCount === 0 || instrCount > GLOBAL_CEILING) bad('V4: instr_count out of global range');
  if (ceiling !== 0 && instrCount > ceiling) bad(`V4: instr_count ${instrCount} > ${PROFILE_NAMES[profile]} ceiling ${ceiling}`);
  if (tableCount > 4) bad('V5: table_count > 4');
  if (ioLaneCount === 0 || ioLaneCount > 32) bad('V6: io_lane_count out of range');

  const expected = ZPROG_HEADER_BYTES + 8 * instrCount + tableBytes + mapBytes;
  if (file.length !== expected) bad(`V1: file length ${file.length} != ${expected}`);

  // CRCs (only meaningful on a length-valid file)
  if (errors.length === 0) {
    const zeroed = Uint8Array.from(file);
    new DataView(zeroed.buffer).setUint32(24, 0, true);
    if (crc32c(0, zeroed) !== bodyCrc) bad('V2: body CRC mismatch');
    const code = file.subarray(ZPROG_HEADER_BYTES, ZPROG_HEADER_BYTES + 8 * instrCount);
    const tables = file.subarray(ZPROG_HEADER_BYTES + 8 * instrCount,
                                 ZPROG_HEADER_BYTES + 8 * instrCount + tableBytes);
    if (programHash(code, tables, instrCount) !== hashField) bad('V2: program hash mismatch');
  }
  if (errors.length > 0) return { ok: false, errors };

  // code
  const instrs: DecodedInstr[] = [];
  for (let i = 0; i < instrCount; i++) instrs.push(unpackWord(r));
  // tables
  const tables: DecodedTable[] = [];
  for (let t = 0; t < tableCount; t++) {
    const kind = r.u8();
    const rsvd = r.u8();
    const n = r.u16();
    if (kind > 1) bad(`V5: table ${t} bad kind ${kind}`);
    if (rsvd !== 0) bad(`V5: table ${t} reserved byte nonzero`);
    if (n < 2 || n > 64) bad(`V5: table ${t} entry count ${n} out of [2,64]`);
    const x = new Int32Array(n), y = new Int32Array(n), dy = new Int32Array(n);
    for (let i = 0; i < n; i++) {
      x[i] = r.i32(); y[i] = r.i32(); dy[i] = r.i32();
      if (i > 0 && x[i]! <= x[i - 1]!) bad(`V5: table ${t} x not strictly increasing at ${i}`);
    }
    if (kind === 1) {
      const step0 = x[1]! - x[0]!;
      for (let i = 1; i < n; i++) {
        if (x[i]! - x[i - 1]! !== step0) bad(`V5: spline table ${t} not uniformly spaced at ${i}`);
      }
    }
    tables.push({ kind, x, y, dy });
  }
  // io map
  const inLanes: DecodedProgram['inLanes'] = [];
  const outLanes: DecodedProgram['outLanes'] = [];
  const laneRaw: { reg: number; kind: number; type: number; nameId: number; min: number; max: number }[] = [];
  for (let i = 0; i < ioLaneCount; i++) {
    laneRaw.push({ reg: r.u8(), kind: r.u8(), type: r.u8(), nameId: r.u8(),
                   min: r.i32(), max: r.i32() });
  }
  // source map
  const srcMap: DecodedProgram['srcMap'] = [];
  for (let i = 0; i < instrCount; i++) {
    srcMap.push({ sourceId: r.u32(), line: r.u16(), col: r.u16() });
  }
  // name pool
  const names: string[] = [];
  for (let i = 0; i < ioLaneCount; i++) {
    let s = '';
    for (;;) {
      if (r.remaining <= 0) { bad('V6: name pool truncated'); break; }
      const ch = r.u8();
      if (ch === 0) break;
      s += String.fromCharCode(ch);
      if (s.length > 64) { bad('V6: name too long'); break; }
    }
    names.push(s);
  }
  if (r.remaining !== 0) bad('V6: trailing bytes in map section');

  // io map semantics (V6)
  for (let i = 0; i < laneRaw.length; i++) {
    const l = laneRaw[i]!;
    if (l.type > 3) bad(`V6: lane ${i} bad type`);
    if (l.nameId !== i) bad(`V6: lane ${i} name_id != ordinal`);
    if (l.kind === 0) {
      inLanes.push({ name: names[i] ?? '', type: l.type, reg: l.reg, min: l.min, max: l.max });
    } else if (l.kind === 1) {
      outLanes.push({ name: names[i] ?? '', type: l.type, reg: l.reg });
    } else {
      bad(`V6: lane ${i} bad kind ${l.kind}`);
    }
  }
  for (let i = 0; i < inLanes.length; i++) {
    if (inLanes[i]!.reg !== i) bad(`V6: input lane ${i} reg ${inLanes[i]!.reg} != ${i}`);
    if (inLanes[i]!.min > inLanes[i]!.max) bad(`V6: input lane ${i} bounds inverted`);
  }
  if (inLanes.length === 0) bad('V6: no input lanes');
  const seen = new Set<number>();
  for (const o of outLanes) {
    if (seen.has(o.reg)) bad(`V6: duplicate output reg ${o.reg}`);
    seen.add(o.reg);
    if (o.reg < inLanes.length) bad(`V6: output reg ${o.reg} is an input register`);
    if (o.reg >= REG_COUNT) bad(`V6: output reg ${o.reg} >= 64`);
  }

  // instruction stream rules (V7–V12) — same list as validateProgram
  const regDefined = new Array<boolean>(REG_COUNT).fill(false);
  for (let i = 0; i < inLanes.length; i++) regDefined[i] = true;
  const inputCount = inLanes.length;
  for (let pc = 0; pc < instrs.length; pc++) {
    const ins = instrs[pc]!;
    validateInstr(pc, ins, { tables, inputCount, regDefined, errors });
  }
  const last = instrs[instrs.length - 1]!;
  if (instrs.length === 0 || last.op !== 0x00) bad('V10: last instruction is not END');
  for (let pc = 0; pc + 1 < instrs.length; pc++) {
    if (instrs[pc]!.op === 0x00) bad('V10: END before last instruction');
  }
  for (const o of outLanes) {
    if (!regDefined[o.reg]) bad(`V12: output reg ${o.reg} never defined`);
  }

  if (errors.length > 0) return { ok: false, errors };
  return {
    ok: true,
    prog: { profile, sourceId, instrs, srcMap, tables, inLanes, outLanes,
            programHash: hashField, bytes: file },
  };
}

/** Per-instruction validator rules shared by decode and unit tests. */
export function validateInstr(pc: number, ins: DecodedInstr, ctx: {
  tables: DecodedTable[]; inputCount: number; regDefined: boolean[]; errors: string[];
}): void {
  const { tables, inputCount, regDefined, errors } = ctx;
  const opName = OP_BY_CODE.get(ins.op);
  if (!opName) { errors.push(`V9: pc ${pc} unknown opcode 0x${ins.op.toString(16)}`); return; }
  const info = OP_INFO[opName];
  const widths = [...info.srcGroups];
  const srcRegs: number[] = [];
  const fieldOf = [ins.a, ins.b, ins.c];
  widths.forEach((w, idx) => {
    for (let k = 0; k < w; k++) srcRegs.push(fieldOf[idx]! + k);
  });
  // unused operand fields must be zero (§2 operand discipline)
  for (let f = widths.length; f < 3; f++) {
    if (fieldOf[f] !== 0) errors.push(`V9: pc ${pc} ${opName} operand field ${f} nonzero`);
  }
  for (const s of srcRegs) {
    if (s >= REG_COUNT) { errors.push(`V7: pc ${pc} src reg ${s} >= 64`); continue; }
    if (!regDefined[s]) errors.push(`V11: pc ${pc} src reg ${s} used before defined`);
  }
  for (let k = 0; k < info.dstWidth; k++) {
    const d = ins.dst + k;
    if (d >= REG_COUNT) errors.push(`V7: pc ${pc} dst reg ${d} >= 64`);
    else if (d < inputCount) errors.push(`V8: pc ${pc} dst reg ${d} is an input`);
    else if (srcRegs.includes(d)) errors.push(`V8: pc ${pc} dst reg ${d} overlaps a source`);
    else regDefined[d] = true;
  }
  // imm discipline (§2, V9)
  switch (info.imm) {
    case 'none': if (ins.imm !== 0) errors.push(`V9: pc ${pc} ${opName} imm nonzero`); break;
    case 'raw': break;
    case 'cmp_mode': if (ins.imm > 5) errors.push(`V9: pc ${pc} CMP mode ${ins.imm} > 5`); break;
    case 'table':
      if (ins.imm >= tables.length) errors.push(`V9: pc ${pc} table id ${ins.imm} >= ${tables.length}`);
      break;
    case 'seed': break;
    case 'rot3_axis':
      if ((ins.imm & ~3) !== 0 || (ins.imm & 3) > 2) {
        errors.push(`V9: pc ${pc} ROT3 axis imm 0x${ins.imm.toString(16)} invalid`);
      }
      break;
  }
}
