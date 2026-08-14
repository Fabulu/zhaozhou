// sample.ts — canonical per-command sample values (golden value recipe).
//
// The generator computes these ONCE and bakes the numbers as literals into
// the generated C++/TS modules; no language ever re-derives them. The golden
// binaries (tests/abi/golden/cmd_*.bin) are the sample records serialized,
// and every implementation byte-compares its own serialization against them
// (spec/capture_format.md 6, protobuf-conformance pattern).
//
// Struct-typed fields get their values from structSample() — a per-struct
// deterministic walk shared by every command that embeds the struct, so the
// C++ `zhao_sample_<struct>()` functions (no arguments) and the golden bytes
// agree by construction.

import { CommandIR, FieldIR, LayoutIR, LeafIR, PRIM_TYPES } from './types.js';

/** FNV-1a 32 — stable, tiny, just for name mixing (NOT a content hash). */
function fnv1a(name: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < name.length; i++) {
    h = ((h ^ name.charCodeAt(i)) * 0x01000193) >>> 0;
  }
  return h >>> 0;
}

function maskFor(prim: string): number {
  const bytes = PRIM_TYPES[prim] ?? 1;
  if (bytes >= 8) return 0xffffffff; // 64-bit values keep low 32 bits in samples
  return (1 << (bytes * 8)) - 1;
}

/**
 * Deterministic scalar sample. Arbitrary but stable; readable in traces:
 * fx16 values are small Q16.16 magnitudes, integers mix the field name with
 * its leaf ordinal and byte offset.
 */
export function sampleScalar(leaf: LeafIR, ordinal: number): number {
  if (leaf.kind === 'pad') return 0; // pads are always zero (validator law)
  if (leaf.kind === 'handle') {
    // allocated handle: generation 0x2A, index = leaf ordinal + 1
    return ((0x2a << 24) | (ordinal + 1)) >>> 0;
  }
  if (leaf.prim === 'fx16') {
    // small Q16.16: (ordinal%8 + 1) + 0x5A17/65536
    return (((ordinal % 8) + 1) << 16 | 0x5a17) >>> 0;
  }
  if (leaf.prim === 'fx32') return 0x000000015a175a17; // Q32.32 ~1.xxx
  const h = fnv1a(leaf.name);
  const v = (h ^ ((ordinal + 1) * 0x9e3779b1) ^ (leaf.offset * 0x85ebca77)) >>> 0;
  return v & maskFor(leaf.prim);
}

/** Per-struct sample: leaf writes relative to the struct start, own ordinals. */
export function structSample(
  ir: LayoutIR,
  structName: string,
): { readonly leaves: readonly { readonly offset: number; readonly size: number; readonly value: number }[] } {
  const s = ir.structs.get(structName);
  if (!s) throw new Error(`unknown struct '${structName}'`);
  const leaves: { offset: number; size: number; value: number }[] = [];
  let ordinal = 0;
  const walk = (fields: readonly FieldIR[], base: number) => {
    for (const f of fields) {
      if (f.kind === 'pad') {
        ordinal += f.count;
        continue;
      }
      if (f.kind === 'struct') {
        walk(ir.structs.get(f.type)!.fields, base + f.offset);
        ordinal += f.leaves.length;
        continue;
      }
      for (const leaf of f.leaves) {
        leaves.push({ offset: base + leaf.offset, size: leaf.size, value: sampleScalar(leaf, ordinal) });
        ordinal++;
      }
    }
  };
  walk(s.fields, 0);
  return { leaves };
}

export interface SampleRecord {
  /** record header sample: opcode, record_bytes, source_id, flags, reserved0 */
  readonly header: {
    readonly opcode: number;
    readonly recordBytes: number;
    readonly sourceId: number;
    readonly flags: number;
    readonly reserved0: number;
  };
  /** leaf payload writes (pads omitted; caller zero-fills) */
  readonly leaves: readonly { readonly offset: number; readonly size: number; readonly value: number }[];
}

/**
 * Build the canonical sample record for a command. source_id uses the §5
 * scheme: kind 5 (command site), module 1, index = command table index.
 * Struct-typed fields embed structSample() (shared across commands).
 */
export function sampleCommand(ir: LayoutIR, cmd: CommandIR): SampleRecord {
  const leaves: { offset: number; size: number; value: number }[] = [];
  let ordinal = 0;
  const walk = (fields: readonly FieldIR[]) => {
    for (const f of fields) {
      if (f.kind === 'pad') {
        ordinal += f.count;
        continue;
      }
      if (f.kind === 'struct') {
        for (const leaf of structSample(ir, f.type).leaves) {
          leaves.push({ offset: f.offset + leaf.offset, size: leaf.size, value: leaf.value });
        }
        ordinal += f.leaves.length;
        continue;
      }
      for (const leaf of f.leaves) {
        leaves.push({ offset: leaf.offset, size: leaf.size, value: sampleScalar(leaf, ordinal) });
        ordinal++;
      }
    }
  };
  walk(cmd.fields);
  return {
    header: {
      opcode: cmd.opcode,
      recordBytes: cmd.recordBytes,
      sourceId: (5 << 28) | (1 << 16) | cmd.index,
      flags: 0,
      reserved0: 0,
    },
    leaves,
  };
}

/** Serialize a sample record to bytes (16-B command header + payload). */
export function sampleRecordBytes(cmd: CommandIR, sample: SampleRecord): Uint8Array {
  const out = new Uint8Array(cmd.recordBytes);
  const dv = new DataView(out.buffer);
  dv.setUint16(0, sample.header.opcode, true);
  dv.setUint16(2, sample.header.recordBytes, true);
  dv.setUint32(4, sample.header.sourceId, true);
  dv.setUint32(8, sample.header.flags, true);
  dv.setUint32(12, sample.header.reserved0, true);
  for (const leaf of sample.leaves) {
    switch (leaf.size) {
      case 1: dv.setUint8(16 + leaf.offset, leaf.value & 0xff); break;
      case 2: dv.setUint16(16 + leaf.offset, leaf.value & 0xffff, true); break;
      case 4: dv.setUint32(16 + leaf.offset, leaf.value >>> 0, true); break;
      default: throw new Error(`sample leaf size ${leaf.size} not supported (v1 ABI has no 8-B leaves)`);
    }
  }
  return out;
}
