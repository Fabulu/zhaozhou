// builder.ts — typed Field IR builder (virtual registers) + offline table
// compiler. Spec: spec/form/field-ir.md §11.1 (builder), §3.15 (dy precompute
// by exact 128-bit signed division — BigInt is allowed HERE in the tool,
// never in the interpreter, §10), §3.20 (smoothstep macro), §7 (profiles).

import {
  FieldTable, IoLane, OpName, OP_INFO, ProfileName, SourceSpan, TablePoint,
  TableKind, LaneType, PROFILE_CEILING,
} from './types.js';

/** Virtual register handle (a value under construction). */
export type Val = number;

interface VInstr {
  op: OpName;
  imm: number;
  span: SourceSpan;
  dst: Val[];        // length = OP_INFO[op].dstWidth
  src: Val[];        // flat, grouped per OP_INFO[op].srcGroups
}

export interface OutputBinding {
  name: string;
  type: LaneType;
  val: Val;
}

export interface BuilderResult {
  profile: ProfileName;
  sourceId: number;
  vInstrs: VInstr[];
  nextV: number;
  tables: FieldTable[];
  inputs: IoLane[];
  outputs: OutputBinding[];
}

/** round_half_up(n/d) for BigInt (qformats.md §4; d > 0). */
function rhuDivBig(n: bigint, d: bigint): bigint {
  return (n + d / 2n) / d;
}

/**
 * Precompute the {x,y,dy} triple table offline (§3.15). Throws on illegal
 * tables (validator V5 mirrors this — the builder is the first line).
 */
export function compileTable(kind: TableKind, points: TablePoint[]): FieldTable {
  if (points.length < 2 || points.length > 64) {
    throw new Error(`table: ${points.length} entries out of [2,64]`);
  }
  for (let i = 1; i < points.length; i++) {
    if (points[i]!.x <= points[i - 1]!.x) {
      throw new Error(`table: x not strictly increasing at ${i}`);
    }
  }
  const dy: number[] = [];
  if (kind === 'curve') {
    for (let i = 0; i + 1 < points.length; i++) {
      const num = BigInt(points[i + 1]!.y - points[i]!.y) << 16n;
      const den = BigInt(points[i + 1]!.x - points[i]!.x);
      dy.push(Number(rhuDivBig(num, den)) | 0);
    }
    dy.push(0);   // last segment has no successor; never selected (x clamped)
  } else {
    const step = BigInt(points[1]!.x - points[0]!.x);
    for (let i = 1; i < points.length; i++) {
      if (BigInt(points[i]!.x - points[i - 1]!.x) !== step) {
        throw new Error('spline table: not uniformly spaced');
      }
    }
    let inv = rhuDivBig(1n << 32n, step);
    if (inv > 0xffffffen) inv = 0xffffffen;
    for (let i = 0; i < points.length; i++) dy.push(Number(inv));
  }
  return { kind, points, dy };
}

/**
 * Generic typed builder. Profile wrappers (EarthBuilder below) pin the
 * input/output records of field-ir.md §7.1.
 */
export class FieldBuilder {
  readonly vInstrs: VInstr[] = [];
  private nextV: number;
  readonly tables: FieldTable[] = [];
  readonly outputs: OutputBinding[] = [];

  constructor(readonly profile: ProfileName, readonly sourceId: number,
              readonly inputs: IoLane[]) {
    // virtual ids [0, n_in) are the input lanes (pinned to R0.. by alloc);
    // fresh dst ids start above them
    this.nextV = inputs.length;
  }

  /** Virtual ids [0, n_in) are the input lanes, pinned to R0.. by alloc. */
  inputVal(i: number): Val { return i; }

  private emit(op: OpName, src: Val[], imm: number, span: SourceSpan): Val[] {
    const w = OP_INFO[op].dstWidth;
    const dst: Val[] = [];
    for (let k = 0; k < w; k++) dst.push(this.nextV++);
    this.vInstrs.push({ op, imm, span, dst, src });
    return dst;
  }

  addTable(kind: TableKind, points: TablePoint[]): number {
    if (this.tables.length >= 4) throw new Error('table: > 4 tables');
    this.tables.push(compileTable(kind, points));
    return this.tables.length - 1;
  }

  // ---- §2 scalar ops ----
  ldc(raw: number, span: SourceSpan): Val { return this.emit('LDC', [], raw | 0, span)[0]!; }
  mov(a: Val, span: SourceSpan): Val { return this.emit('MOV', [a], 0, span)[0]!; }
  add(a: Val, b: Val, span: SourceSpan): Val { return this.emit('ADD', [a, b], 0, span)[0]!; }
  sub(a: Val, b: Val, span: SourceSpan): Val { return this.emit('SUB', [a, b], 0, span)[0]!; }
  mul(a: Val, b: Val, span: SourceSpan): Val { return this.emit('MUL', [a, b], 0, span)[0]!; }
  mad(a: Val, b: Val, c: Val, span: SourceSpan): Val {
    return this.emit('MAD', [a, b, c], 0, span)[0]!;
  }
  min(a: Val, b: Val, span: SourceSpan): Val { return this.emit('MIN', [a, b], 0, span)[0]!; }
  max(a: Val, b: Val, span: SourceSpan): Val { return this.emit('MAX', [a, b], 0, span)[0]!; }
  abs(a: Val, span: SourceSpan): Val { return this.emit('ABS', [a], 0, span)[0]!; }
  clamp(v: Val, lo: Val, hi: Val, span: SourceSpan): Val {
    return this.emit('CLAMP', [v, lo, hi], 0, span)[0]!;
  }
  select(cond: Val, a: Val, b: Val, span: SourceSpan): Val {
    // SELECT: dst ← C≠0 ? A : B (§3.8) — srcs [a, b, cond]
    return this.emit('SELECT', [a, b, cond], 0, span)[0]!;
  }
  cmp(a: Val, b: Val, mode: 0 | 1 | 2 | 3 | 4 | 5, span: SourceSpan): Val {
    return this.emit('CMP', [a, b], mode, span)[0]!;
  }

  // ---- §2 vector ops (adjacency handled by the allocator, §11.2) ----
  dot2(a0: Val, a1: Val, b0: Val, b1: Val, span: SourceSpan): Val {
    return this.emit('DOT2', [a0, a1, b0, b1], 0, span)[0]!;
  }
  dot3(a0: Val, a1: Val, a2: Val, b0: Val, b1: Val, b2: Val, span: SourceSpan): Val {
    return this.emit('DOT3', [a0, a1, a2, b0, b1, b2], 0, span)[0]!;
  }
  len2(x: Val, y: Val, span: SourceSpan): Val {
    return this.emit('LEN2', [x, y], 0, span)[0]!;
  }
  len3(x: Val, y: Val, z: Val, span: SourceSpan): Val {
    return this.emit('LEN3', [x, y, z], 0, span)[0]!;
  }
  dist2(ax: Val, ay: Val, bx: Val, by: Val, span: SourceSpan): Val {
    return this.emit('DIST2', [ax, ay, bx, by], 0, span)[0]!;
  }
  normalize2(x: Val, y: Val, span: SourceSpan): [Val, Val] {
    const d = this.emit('NORMALIZE2', [x, y], 0, span);
    return [d[0]!, d[1]!];
  }
  normalize3(x: Val, y: Val, z: Val, span: SourceSpan): [Val, Val, Val] {
    const d = this.emit('NORMALIZE3', [x, y, z], 0, span);
    return [d[0]!, d[1]!, d[2]!];
  }
  rcp(a: Val, span: SourceSpan): Val { return this.emit('RCP', [a], 0, span)[0]!; }
  sin(a: Val, span: SourceSpan): Val { return this.emit('SIN', [a], 0, span)[0]!; }
  cos(a: Val, span: SourceSpan): Val { return this.emit('COS', [a], 0, span)[0]!; }
  curve(tid: number, a: Val, span: SourceSpan): Val {
    return this.emit('CURVE', [a], tid, span)[0]!;
  }
  spline(tid: number, a: Val, span: SourceSpan): Val {
    return this.emit('SPLINE', [a], tid, span)[0]!;
  }
  dcurve(tid: number, a: Val, span: SourceSpan): Val {
    return this.emit('DCURVE', [a], tid, span)[0]!;
  }
  noise2(x: Val, y: Val, seed: number, span: SourceSpan): [Val, Val] {
    const d = this.emit('NOISE2', [x, y], seed >>> 0, span);
    return [d[0]!, d[1]!];
  }
  ring(d: Val, r0: Val, r1: Val, span: SourceSpan): Val {
    return this.emit('RING', [d, r0, r1], 0, span)[0]!;
  }
  ridge(x: Val, y: Val, seed: number, span: SourceSpan): Val {
    return this.emit('RIDGE', [x, y], seed >>> 0, span)[0]!;
  }
  rot2(x: Val, y: Val, ang: Val, span: SourceSpan): [Val, Val] {
    const d = this.emit('ROT2', [x, y, ang], 0, span);
    return [d[0]!, d[1]!];
  }
  rot3(x: Val, y: Val, z: Val, ang: Val, axis: 0 | 1 | 2, span: SourceSpan):
      [Val, Val, Val] {
    const d = this.emit('ROT3', [x, y, z, ang], axis, span);
    return [d[0]!, d[1]!, d[2]!];
  }

  // ---- §3.20 smoothstep macro (NOT an opcode — A1b/ops.yml A3e) ----
  smoothstep(e0: Val, e1: Val, x: Val, span: SourceSpan): Val {
    const d = this.sub(e1, e0, span);
    const r = this.rcp(d, span);
    const t = this.mul(this.sub(x, e0, span), r, span);
    const c0 = this.ldc(0, span);
    const c1 = this.ldc(1 << 16, span);
    const tc = this.clamp(t, c0, c1, span);
    const t2 = this.mul(tc, tc, span);
    const k2 = this.ldc(2 << 16, span);
    const u = this.mul(k2, tc, span);
    const k3 = this.ldc(3 << 16, span);
    const w = this.sub(k3, u, span);
    return this.mul(t2, w, span);
  }

  end(span: SourceSpan): void {
    this.emit('END', [], 0, span);
  }

  output(name: string, type: LaneType, val: Val): void {
    if (this.outputs.some((o) => o.name === name)) {
      throw new Error(`duplicate output ${name}`);
    }
    this.outputs.push({ name, type, val });
  }

  get inputCount(): number { return this.inputs.length; }
  get ceiling(): number { return PROFILE_CEILING[this.profile]; }
}
