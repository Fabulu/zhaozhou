// types.ts — AST + LayoutIR shapes for the .zidl pipeline (spec/capture_format.md 1).

export interface SourcePos {
  readonly line: number;
  readonly col: number;
}

export class ZidlError extends Error {
  constructor(message: string, readonly pos: SourcePos) {
    super(`commands.zidl:${pos.line}:${pos.col}: ${message}`);
  }
}

// ---- primitive wire types (capture_format.md 1.1 rule 6) ------------------
// fx16 = Q16.16 in a 4-byte int32 container; fx32 = Q32.32 in 8 bytes
// (spec/qformats.md is the authority on the Q formats themselves).
export const PRIM_TYPES: Readonly<Record<string, number>> = {
  u8: 1, u16: 2, u32: 4, u64: 8,
  i8: 1, i16: 2, i32: 4, i64: 8,
  fx16: 4, fx32: 8,
  pad: 1,
};

export const INT_TYPES: ReadonlySet<string> = new Set([
  'u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64', 'fx16', 'fx32',
]);

// ---- AST -------------------------------------------------------------------

export interface AbiDecl {
  readonly name: string;
  version: number;
  endian: 'little' | 'big';
  commandAlignment: number;
  opcodeWidth: 'u8' | 'u16';
}

export interface ConstDecl {
  readonly type: 'u8' | 'u16' | 'u32' | 'u64';
  readonly name: string;
  readonly value: number;
}

export interface EnumEntryAst {
  readonly name: string;
  readonly value: number;
}

export interface EnumDecl {
  readonly name: string;
  readonly type: string; // backing prim type
  readonly entries: readonly EnumEntryAst[];
}

export interface FieldAst {
  readonly name: string;
  readonly type: string; // prim | struct | enum name
  readonly count: number; // fixed array length (1 = scalar)
  readonly isPad: boolean;
  readonly handleKind?: string;
  readonly pos: SourcePos;
}

export interface StructDecl {
  readonly name: string;
  readonly fields: readonly FieldAst[];
}

export interface CommandDecl {
  readonly name: string;
  readonly opcode: number;
  readonly implemented: boolean;
  readonly fields: readonly FieldAst[];
  readonly pos: SourcePos;
}

export interface ZidlAst {
  readonly abi: AbiDecl;
  readonly consts: readonly ConstDecl[];
  readonly enums: readonly EnumDecl[];
  readonly structs: readonly StructDecl[];
  readonly commands: readonly CommandDecl[];
}

// ---- LayoutIR (the single layout truth; emitters never re-derive) ----------

export type FieldKind = 'scalar' | 'pad' | 'struct' | 'handle' | 'enum';

export interface FieldIR {
  readonly name: string;
  readonly type: string; // wire prim type ('u32', 'fx16', ...) or struct name
  readonly kind: FieldKind;
  readonly count: number; // array length
  readonly offset: number; // byte offset of element 0 within the owning unit
  readonly size: number; // total bytes incl. array
  readonly handleKind?: string;
  readonly pos: SourcePos;
  /** all leaf scalar writes, flattened, for byte-exact goldens */
  readonly leaves: readonly LeafIR[];
}

export interface LeafIR {
  readonly name: string; // dotted path, e.g. "footprint.x0" / "parameters[3]"
  readonly prim: string; // leaf wire type
  readonly offset: number;
  readonly size: number;
  readonly kind: FieldKind;
}

export interface StructIR {
  readonly name: string;
  readonly size: number;
  readonly fields: readonly FieldIR[];
}

export interface CommandIR {
  readonly name: string; // PascalCase
  readonly snake: string; // snake_case
  readonly lowerCamel: string;
  readonly opcode: number;
  readonly opcodeHex: string; // 0x%04X
  readonly implemented: boolean;
  readonly recordBytes: number; // 16-byte header + payload
  readonly fields: readonly FieldIR[]; // payload fields only
  readonly index: number; // position in the command table
}

export interface LayoutIR {
  readonly abi: AbiDecl;
  readonly consts: readonly ConstDecl[];
  readonly enums: readonly EnumDecl[];
  readonly structs: ReadonlyMap<string, StructIR>;
  readonly commands: readonly CommandIR[];
  /** canonical, deterministic text form — hashed into .zcap ABI_INFO identity */
  readonly identityText: string;
}

export function snakeCase(name: string): string {
  return name.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase();
}

export function lowerCamel(name: string): string {
  return name.charAt(0).toLowerCase() + name.slice(1);
}

export function upperSnake(name: string): string {
  return snakeCase(name).toUpperCase();
}

export function hex4(n: number): string {
  return `0x${n.toString(16).toUpperCase().padStart(4, '0')}`;
}
