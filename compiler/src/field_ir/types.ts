// types.ts — Field IR v1 types + frozen op metadata.
//
// Spec: spec/form/field-ir.md (FIELD_IR_VERSION 1)
//   §1.1  64-bit word layout (opcode|dst|srcA|srcB|srcC|imm32)
//   §2    opcode table v1 (frozen; DCURVE 0x1D added, SMOOTHSTEP demoted to
//         macro — slot 0x20 reserved)
//   §1.3  adjacent-register convention (group widths below)
//   §7    profiles, I/O records, ceilings
//
// Numeric law: spec/qformats.md (QFMT_VERSION 1).

export const FIELD_IR_VERSION = 1;

export type ProfileId = 0 | 1 | 2 | 3 | 4;
export const PROFILE_NAMES = ['earth', 'warp', 'flow', 'formation', 'stamp'] as const;
export type ProfileName = (typeof PROFILE_NAMES)[number];

/** Provisional ceilings (field-ir.md §7.3; global hard ceiling 64). */
export const PROFILE_CEILING: Record<ProfileName, number> = {
  earth: 32, warp: 48, flow: 48, formation: 64, stamp: 32,
};
export const GLOBAL_CEILING = 64;

export const REG_COUNT = 64;

/** I/O lane types (field-ir.md §5.3). */
export type LaneType = 'fx' | 'unit' | 'angle' | 'u32';
export const LANE_TYPE_CODE: Record<LaneType, number> = { fx: 0, unit: 1, angle: 2, u32: 3 };

export type TableKind = 'curve' | 'spline';
export const TABLE_KIND_CODE: Record<TableKind, number> = { curve: 0, spline: 1 };

/** A {x,y} control point as authored; dy is precomputed by the table compiler. */
export interface TablePoint { x: number; y: number }  // raw fx ints
export interface FieldTable {
  kind: TableKind;
  /** As authored (x,y); dy is derived (field-ir.md §3.15) and frozen at build. */
  points: TablePoint[];
  /** Precomputed offline (curve: slope; spline: Q16.16 of 1/Δx). Filled by the builder. */
  dy: number[];
}

export interface SourceSpan {
  sourceId: number;  // capture_format.md §5 (kind 3 = field program)
  line: number;      // 1-based
  col: number;       // 1-based
}

// ---------------------------------------------------------------------------
// Frozen opcode table (field-ir.md §2)
// ---------------------------------------------------------------------------

export type OpName =
  | 'END' | 'MOV' | 'LDC' | 'ADD' | 'SUB' | 'MUL' | 'MAD' | 'MIN' | 'MAX'
  | 'ABS' | 'CLAMP' | 'SELECT' | 'CMP'
  | 'DOT2' | 'DOT3' | 'LEN2' | 'LEN3' | 'DIST2' | 'NORMALIZE2' | 'NORMALIZE3'
  | 'RCP' | 'SIN' | 'COS' | 'CURVE' | 'SPLINE' | 'NOISE2' | 'DCURVE'
  | 'RING' | 'RIDGE' | 'ROT2' | 'ROT3';

export type CostClass = 'ALU' | 'MUL' | 'TABLE' | 'NOISE' | 'SPECIAL';

export type ImmUse = 'none' | 'raw' | 'cmp_mode' | 'table' | 'seed' | 'rot3_axis';

export interface OpInfo {
  code: number;
  cls: CostClass;
  /** destination group width (field-ir.md §1.3) */
  dstWidth: number;
  /** source groups: lengths in operand order [A..., B..., C...] */
  srcGroups: number[];
  imm: ImmUse;
}

const OP: Record<OpName, OpInfo> = {
  END:     { code: 0x00, cls: 'ALU',     dstWidth: 0, srcGroups: [],          imm: 'none' },
  MOV:     { code: 0x01, cls: 'ALU',     dstWidth: 1, srcGroups: [1],         imm: 'none' },
  LDC:     { code: 0x02, cls: 'ALU',     dstWidth: 1, srcGroups: [],          imm: 'raw' },
  ADD:     { code: 0x03, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1],      imm: 'none' },
  SUB:     { code: 0x04, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1],      imm: 'none' },
  MUL:     { code: 0x05, cls: 'MUL',     dstWidth: 1, srcGroups: [1, 1],      imm: 'none' },
  MAD:     { code: 0x06, cls: 'MUL',     dstWidth: 1, srcGroups: [1, 1, 1],   imm: 'none' },
  MIN:     { code: 0x07, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1],      imm: 'none' },
  MAX:     { code: 0x08, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1],      imm: 'none' },
  ABS:     { code: 0x09, cls: 'ALU',     dstWidth: 1, srcGroups: [1],         imm: 'none' },
  CLAMP:   { code: 0x0a, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1, 1],   imm: 'none' },
  SELECT:  { code: 0x0b, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1, 1],   imm: 'none' },
  CMP:     { code: 0x0c, cls: 'ALU',     dstWidth: 1, srcGroups: [1, 1],      imm: 'cmp_mode' },
  DOT2:    { code: 0x10, cls: 'MUL',     dstWidth: 1, srcGroups: [2, 2],      imm: 'none' },
  DOT3:    { code: 0x11, cls: 'MUL',     dstWidth: 1, srcGroups: [3, 3],      imm: 'none' },
  LEN2:    { code: 0x12, cls: 'SPECIAL', dstWidth: 1, srcGroups: [2],         imm: 'none' },
  LEN3:    { code: 0x13, cls: 'SPECIAL', dstWidth: 1, srcGroups: [3],         imm: 'none' },
  DIST2:   { code: 0x14, cls: 'SPECIAL', dstWidth: 1, srcGroups: [2, 2],      imm: 'none' },
  NORMALIZE2: { code: 0x15, cls: 'SPECIAL', dstWidth: 2, srcGroups: [2],      imm: 'none' },
  NORMALIZE3: { code: 0x16, cls: 'SPECIAL', dstWidth: 3, srcGroups: [3],      imm: 'none' },
  RCP:     { code: 0x17, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'none' },
  SIN:     { code: 0x18, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'none' },
  COS:     { code: 0x19, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'none' },
  CURVE:   { code: 0x1a, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'table' },
  SPLINE:  { code: 0x1b, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'table' },
  NOISE2:  { code: 0x1c, cls: 'NOISE',   dstWidth: 2, srcGroups: [2],         imm: 'seed' },
  DCURVE:  { code: 0x1d, cls: 'TABLE',   dstWidth: 1, srcGroups: [1],         imm: 'table' },
  RING:    { code: 0x21, cls: 'SPECIAL', dstWidth: 1, srcGroups: [1, 1, 1],   imm: 'none' },
  RIDGE:   { code: 0x22, cls: 'NOISE',   dstWidth: 1, srcGroups: [1, 1],      imm: 'seed' },
  ROT2:    { code: 0x28, cls: 'SPECIAL', dstWidth: 2, srcGroups: [2, 1],      imm: 'none' },
  ROT3:    { code: 0x29, cls: 'SPECIAL', dstWidth: 3, srcGroups: [3, 1],      imm: 'rot3_axis' },
};

export const OP_INFO: Record<OpName, OpInfo> = OP;

const BY_CODE = new Map<number, OpName>(
  (Object.keys(OP) as OpName[]).map((n) => [OP[n]!.code, n]),
);
export function opFromCode(code: number): OpName | undefined {
  return BY_CODE.get(code);
}

/** Provisional per-class cycle estimates (field-ir.md §9). */
export const CLASS_CYCLES: Record<CostClass, number> = {
  ALU: 1, MUL: 2, TABLE: 3, NOISE: 2, SPECIAL: 5,
};

// ---------------------------------------------------------------------------
// Physical program (post-allocation; what gets validated + serialized)
// ---------------------------------------------------------------------------

export interface PhysOp {
  op: OpName;
  dst: number;  // group start (width from OP_INFO)
  a: number;    // srcA (group start)
  b: number;
  c: number;
  imm: number;  // int32
  span: SourceSpan;
}

export interface IoLane {
  name: string;
  type: LaneType;
  reg: number;
  /** raw fx/u32 ints; inputs only (outputs carry 0,0) */
  min: number;
  max: number;
}

export interface CostSummary {
  instrCount: number;
  byClass: Record<CostClass, number>;
  cycles: number;
  dsp: number;
  tableBytes: number;
  regHighWater: number;
}

export interface FieldProgram {
  version: 1;
  profile: ProfileName;
  sourceId: number;
  code: PhysOp[];        // ends with END (validated)
  tables: FieldTable[];
  inputs: IoLane[];
  outputs: IoLane[];
  cost: CostSummary;
}
