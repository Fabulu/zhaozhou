// alloc.ts — linear-scan register allocator with adjacency coalescing.
// Spec: spec/form/field-ir.md §11.2 (no spilling; > 64 live registers is a
// compile error), §1.2/§1.3 (inputs pinned to R0..; vector groups adjacent),
// §4 V6/V8 (outputs never input regs; dst groups never overlap sources).
//
// Forward single pass over the virtual program:
//   - inputs stay at R0..R(n_in−1), never freed;
//   - every virtual has a use count; its physical register frees at its last
//     read (linear scan over live ranges, in program order);
//   - a source group that is not already physically adjacent is coalesced by
//     inserting MOVs into fresh adjacent scratch registers (§11.2);
//   - destination groups allocate consecutive runs (lowest-first).

import {
  FieldProgram, IoLane, OP_INFO, PhysOp, SourceSpan,
} from './types.js';
import { FieldBuilder } from './builder.js';
import { computeCost } from './cost.js';

interface Allocated {
  code: PhysOp[];
  outputs: IoLane[];
  regHighWater: number;
}

export function allocateProgram(b: FieldBuilder): Allocated {
  const nIn = b.inputCount;
  const inUse = new Array<boolean>(64).fill(false);
  for (let i = 0; i < nIn; i++) inUse[i] = true;
  let highWater = nIn;

  const physOf = new Map<number, number>();
  for (let i = 0; i < nIn; i++) physOf.set(i, i);

  // remaining reads per virtual (MOV coalescing reads count too)
  const usesLeft = new Map<number, number>();
  const bump = (v: number) => usesLeft.set(v, (usesLeft.get(v) ?? 0) + 1);
  for (const ins of b.vInstrs) for (const v of ins.src) bump(v);
  // outputs are read "at END": pin their registers until the end
  const pinned = new Set<number>(b.outputs.map((o) => o.val));
  for (const v of pinned) bump(v);

  const freeReg = (phys: number) => {
    if (phys < nIn) return;                    // inputs are never freed
    inUse[phys] = false;
  };

  /** lowest consecutive free run of w scratch regs, or -1 */
  const allocRun = (w: number): number => {
    if (w === 0) return 0;
    let run = 0;
    for (let r = nIn; r < 64; r++) {
      run = inUse[r] ? 0 : run + 1;
      if (run === w) return r - w + 1;
    }
    return -1;
  };

  const code: PhysOp[] = [];
  const emitMov = (dst: number, src: number, span: SourceSpan) => {
    code.push({ op: 'MOV', dst, a: src, b: 0, c: 0, imm: 0, span });
  };

  for (const ins of b.vInstrs) {
    const info = OP_INFO[ins.op];
    const groups: number[][] = [];
    let idx = 0;
    for (const w of info.srcGroups) {
      groups.push(ins.src.slice(idx, idx + w));
      idx += w;
    }

    // (a) coalesce source groups into adjacent physical runs
    for (const g of groups) {
      if (g.length < 2) continue;
      const p0 = physOf.get(g[0]!)!;
      const adjacent = g.every((v, k) => physOf.get(v) === p0 + k);
      if (adjacent) continue;
      const base = allocRun(g.length);
      if (base < 0) {
        throw new Error(`register pressure: cannot coalesce group of ${g.length}`);
      }
      for (let k = 0; k < g.length; k++) {
        const v = g[k]!;
        const old = physOf.get(v)!;
        emitMov(base + k, old, ins.span);
        physOf.set(v, base + k);
        inUse[base + k] = true;
        if (base + k >= highWater) highWater = base + k + 1;
        if (old >= nIn) {
          inUse[old] = false;                   // value moved; old copy unused
        }
      }
    }

    // (b) destination group (fresh consecutive run; V8: no source overlap —
    // guaranteed because the run is freshly allocated and sources are live)
    const dstBase = allocRun(info.dstWidth);
    if (dstBase < 0 && info.dstWidth > 0) {
      throw new Error(`register pressure: dst group of ${info.dstWidth}`);
    }
    for (let k = 0; k < info.dstWidth; k++) {
      inUse[dstBase + k] = true;
      physOf.set(ins.dst[k]!, dstBase + k);
      if (dstBase + k >= highWater) highWater = dstBase + k + 1;
    }

    // (c) emit the physical instruction
    const a = groups[0]?.[0] ?? 0;
    const bb = groups[1]?.[0] ?? 0;
    const cc = groups[2]?.[0] ?? 0;
    code.push({
      op: ins.op, dst: ins.dst.length ? dstBase : 0,
      a: physOf.get(a) ?? 0, b: physOf.get(bb) ?? 0, c: physOf.get(cc) ?? 0,
      imm: ins.imm, span: ins.span,
    });

    // (d) retire virtuals whose reads are exhausted (end of live range)
    const retire = (v: number) => {
      const left = (usesLeft.get(v) ?? 0) - 1;
      usesLeft.set(v, left);
      if (left <= 0 && !pinned.has(v)) {
        const p = physOf.get(v);
        if (p !== undefined) freeReg(p);
      }
    };
    for (const g of groups) for (const v of g) retire(v);
  }

  // resolve outputs (must still be live — pinned)
  const outputs: IoLane[] = b.outputs.map((o) => {
    const reg = physOf.get(o.val);
    if (reg === undefined) throw new Error(`output ${o.name} has no register`);
    return { name: o.name, type: o.type, reg, min: 0, max: 0 };
  });

  return { code, outputs, regHighWater: highWater };
}

/** Build a complete FieldProgram: allocate, cost, and freeze outputs. */
export function buildProgram(b: FieldBuilder): FieldProgram {
  const { code, outputs, regHighWater } = allocateProgram(b);
  return {
    version: 1,
    profile: b.profile,
    sourceId: b.sourceId,
    code,
    tables: b.tables,
    inputs: b.inputs,
    outputs,
    cost: computeCost(code, b.tables, regHighWater),
  };
}
