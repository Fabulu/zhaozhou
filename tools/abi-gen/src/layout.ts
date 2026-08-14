// layout.ts — semantic pass + THE single layout calculator (capture_format.md
// 1.1). Everything downstream (all five emitters + goldens) consumes the
// LayoutIR produced here; layout is never re-derived anywhere else, which
// makes tri-language byte-identity structural rather than lucky.

import {
  CommandIR, CommandDecl, FieldAst, FieldIR, LeafIR, LayoutIR, PRIM_TYPES,
  StructIR, ZidlAst, ZidlError, hex4, lowerCamel, snakeCase,
} from './types.js';

export interface SemanticOptions {
  /** ratified opcode ranges (capture_format.md 1.2): [lo, hi, label] */
  readonly opcodeRanges: readonly (readonly [number, number, string])[];
}

/** Ratified opcode ranges (capture_format.md 1.2). */
export const OPCODE_RANGES: readonly (readonly [number, number, string])[] = [
  [0x0000, 0x00ff, 'frame control / views / presentation'],
  [0x0200, 0x02ff, 'terrain / surface'],
  [0x0300, 0x03ff, 'forms / populations / procedural'],
  [0x0400, 0x04ff, 'audio'],
  [0xf000, 0xf0ff, 'bootstrap / debug umbrella'],
];

export const DEBUG_OPCODE_LO = 0xf000;
export const DEBUG_OPCODE_HI = 0xf0ff;

function primSize(t: string): number {
  return PRIM_TYPES[t] ?? 0;
}

/** natural alignment capped at 4 (capture_format.md 1.1 rule 2) */
function fieldAlignment(size: number): number {
  return Math.min(size, 4);
}

function padNeeded(offset: number, size: number): number {
  const a = fieldAlignment(size);
  return (a - (offset % a)) % a;
}

function isPowerOfTwo(n: number): boolean {
  return n > 0 && (n & (n - 1)) === 0;
}

/**
 * Compute the flat byte layout of one field list (a struct body or a command
 * payload). Implicit padding is a hard error — the .zidl must be the complete
 * truth (capture_format.md 1.1 rule 2). Alignment applies to the ELEMENT size
 * (arrays pack at element stride, rule 3).
 */
function layoutFields(
  fields: readonly FieldAst[],
  structs: ReadonlyMap<string, StructIR>, // already-laid-out struct IRs
  enums: ReadonlyMap<string, string>, // enum name -> backing prim type
  unitName: string,
): { fields: FieldIR[]; size: number } {
  const out: FieldIR[] = [];
  let off = 0;
  const seen = new Set<string>();

  const emitField = (f: FieldAst) => {
    // pads: explicit zero bytes; anonymous `pad[n]` gets a unique per-unit name
    if (f.isPad) {
      let name = f.name;
      let k = 0;
      while (seen.has(name)) name = `pad_${++k}`;
      seen.add(name);
      const leaves: LeafIR[] = [];
      for (let e = 0; e < f.count; e++) {
        leaves.push({ name: `${name}[${e}]`, prim: 'pad', offset: off + e, size: 1, kind: 'pad' });
      }
      out.push({
        name, type: 'pad', kind: 'pad', count: f.count,
        offset: off, size: f.count, handleKind: undefined, pos: f.pos, leaves,
      });
      off += f.count;
      return;
    }

    if (seen.has(f.name)) {
      throw new ZidlError(`duplicate field '${f.name}' in ${unitName}`, f.pos);
    }
    seen.add(f.name);

    const kind: FieldIR['kind'] = f.type === 'handle32' ? 'handle'
      : enums.has(f.type) ? 'enum'
        : structs.has(f.type) ? 'struct' : 'scalar';

    const elemSize = kind === 'handle' ? 4
      : kind === 'enum' ? primSize(enums.get(f.type)!)
        : kind === 'struct' ? structs.get(f.type)!.size
          : primSize(f.type);
    if (elemSize === 0) {
      throw new ZidlError(`unknown field type '${f.type}' in ${unitName}`, f.pos);
    }

    const pad = padNeeded(off, elemSize);
    if (pad !== 0) {
      throw new ZidlError(
        `${unitName}.${f.name}: implicit padding of ${pad} byte(s) before offset ${off} ` +
        `(element size ${elemSize}) — insert explicit pad[${pad}]`,
        f.pos,
      );
    }

    const leaves: LeafIR[] = [];
    if (kind === 'struct') {
      const s = structs.get(f.type)!;
      for (let k = 0; k < f.count; k++) {
        const base = off + k * s.size;
        for (const lf of s.fields) {
          for (const leaf of lf.leaves) {
            leaves.push({
              name: `${f.name}${f.count > 1 ? `[${k}]` : ''}.${leaf.name}`,
              prim: leaf.prim, offset: base + leaf.offset, size: leaf.size, kind: leaf.kind,
            });
          }
        }
      }
    } else {
      for (let k = 0; k < f.count; k++) {
        leaves.push({
          name: `${f.name}${f.count > 1 ? `[${k}]` : ''}`,
          prim: kind === 'enum' ? enums.get(f.type)! : (kind === 'handle' ? 'u32' : f.type),
          offset: off + k * elemSize, size: elemSize, kind,
        });
      }
    }

    out.push({
      name: f.name, type: f.type, kind, count: f.count,
      offset: off, size: elemSize * f.count, handleKind: f.handleKind, pos: f.pos, leaves,
    });
    off += elemSize * f.count;
  };

  for (const f of fields) {
    emitField(f);
  }
  return { fields: out, size: off };
}

/** Canonical identity text — hashed into .zcap ABI_INFO (capture_format.md 4.2). */
function identityText(ir: Omit<LayoutIR, 'identityText'>): string {
  const lines: string[] = [];
  lines.push(
    `abi ${ir.abi.name} v${ir.abi.version} ${ir.abi.endian} ` +
    `align=${ir.abi.commandAlignment} opw=${ir.abi.opcodeWidth}`,
  );
  for (const c of ir.consts) lines.push(`const ${c.type} ${c.name} = ${c.value}`);
  for (const e of ir.enums) {
    lines.push(`enum ${e.name}:${e.type} { ${e.entries.map((x) => `${x.name}=${x.value}`).join(', ')} }`);
  }
  for (const s of ir.structs.values()) {
    lines.push(
      `struct ${s.name}(${s.size}) { ` +
      s.fields.map((f) => `${f.type}${f.count > 1 ? `[${f.count}]` : ''} ${f.name}@${f.offset}`).join('; ') +
      ' }',
    );
  }
  for (const c of ir.commands) {
    lines.push(
      `command ${c.name} ${c.opcodeHex} ${c.implemented ? 'implemented' : 'reserved'} ` +
      `record=${c.recordBytes} { ` +
      c.fields.map((f) => `${f.type}${f.count > 1 ? `[${f.count}]` : ''} ${f.name}@${f.offset}`).join('; ') +
      ' }',
    );
  }
  return lines.join('\n') + '\n';
}

export function semantic(ast: ZidlAst, opts: SemanticOptions = { opcodeRanges: OPCODE_RANGES }): LayoutIR {
  const { abi } = ast;

  // ---- abi-level rules -------------------------------------------------------
  if (abi.version <= 0) throw new ZidlError('abi.version must be >= 1', { line: 1, col: 1 });
  if (abi.endian !== 'little') {
    throw new ZidlError('Phase 1 supports endian little only', { line: 1, col: 1 });
  }
  if (!isPowerOfTwo(abi.commandAlignment) || abi.commandAlignment < 16) {
    throw new ZidlError('command_alignment must be a power of two >= 16', { line: 1, col: 1 });
  }
  if (abi.opcodeWidth !== 'u16') {
    throw new ZidlError('Phase 1 supports opcode_width u16 only', { line: 1, col: 1 });
  }

  // ---- name registries -------------------------------------------------------
  const structDecls = new Map(ast.structs.map((s) => [s.name, s] as const));
  const enumTypes = new Map(ast.enums.map((e) => [e.name, e.type] as const));
  const globalNames = new Set<string>([
    ...ast.consts.map((c) => c.name),
    ...ast.enums.map((e) => e.name),
    ...ast.structs.map((s) => s.name),
  ]);

  for (const e of ast.enums) {
    const seen = new Set<number>();
    for (const entry of e.entries) {
      if (seen.has(entry.value)) {
        throw new ZidlError(`enum '${e.name}': duplicate value ${entry.value}`, { line: 1, col: 1 });
      }
      seen.add(entry.value);
    }
  }

  // ---- structs: layout in dependency order (no recursion allowed) ------------
  const structIR = new Map<string, StructIR>();
  const pending = [...ast.structs];
  let guard = pending.length * (pending.length + 1);
  while (pending.length > 0) {
    if (--guard < 0) {
      throw new ZidlError('struct dependency cycle or unresolvable struct reference', { line: 1, col: 1 });
    }
    let progressed = false;
    for (let i = 0; i < pending.length; i++) {
      const s = pending[i]!;
      const deps = s.fields
        .filter((f) => !f.isPad && f.type !== 'handle32' && structDecls.has(f.type))
        .map((f) => f.type);
      if (deps.every((d) => structIR.has(d))) {
        const { fields, size } = layoutFields(s.fields, structIR, enumTypes, `struct ${s.name}`);
        if (size <= 0) {
          throw new ZidlError(`struct '${s.name}' is empty`, { line: 1, col: 1 });
        }
        structIR.set(s.name, { name: s.name, size, fields });
        pending.splice(i, 1);
        i--;
        progressed = true;
      }
    }
    if (!progressed && pending.length > 0) {
      const s = pending[0]!;
      for (const f of s.fields) {
        if (structDecls.has(f.type) && !structIR.has(f.type)) {
          throw new ZidlError(`struct '${s.name}' references unknown or cyclic struct '${f.type}'`, f.pos);
        }
      }
      throw new ZidlError(`struct '${s.name}' has unresolvable fields`, { line: 1, col: 1 });
    }
  }

  // ---- commands ---------------------------------------------------------------
  const opcodes = new Map<number, string>();
  const commands: CommandIR[] = ast.commands.map((cmd: CommandDecl, index) => {
    if (globalNames.has(cmd.name)) {
      throw new ZidlError(`command name '${cmd.name}' collides with another declaration`, cmd.pos);
    }
    globalNames.add(cmd.name);

    if (opcodes.has(cmd.opcode)) {
      throw new ZidlError(
        `duplicate opcode ${hex4(cmd.opcode)} ('${opcodes.get(cmd.opcode)}' and '${cmd.name}')`,
        cmd.pos,
      );
    }
    opcodes.set(cmd.opcode, cmd.name);

    const inRange = opts.opcodeRanges.some(([lo, hi]) => cmd.opcode >= lo && cmd.opcode <= hi);
    if (!inRange) {
      throw new ZidlError(
        `opcode ${hex4(cmd.opcode)} for '${cmd.name}' is outside every ratified range ` +
        `(${opts.opcodeRanges.map(([lo, hi]) => `${hex4(lo)}-${hex4(hi)}`).join(', ')})`,
        cmd.pos,
      );
    }
    const isDebug = cmd.opcode >= DEBUG_OPCODE_LO && cmd.opcode <= DEBUG_OPCODE_HI;
    if (isDebug && cmd.implemented) {
      throw new ZidlError(
        `debug opcode ${hex4(cmd.opcode)} ('${cmd.name}') can never be an implemented ` +
        'game-facing command (plan 1.E)',
        cmd.pos,
      );
    }

    const { fields, size } = layoutFields(cmd.fields, structIR, enumTypes, `command ${cmd.name}`);
    const record = 16 + size;
    if (record % abi.commandAlignment !== 0) {
      const need = abi.commandAlignment - (record % abi.commandAlignment);
      throw new ZidlError(
        `command '${cmd.name}': record size ${record} is not a multiple of ` +
        `command_alignment ${abi.commandAlignment} — add explicit pad[${need}]`,
        cmd.pos,
      );
    }

    return {
      name: cmd.name,
      snake: snakeCase(cmd.name),
      lowerCamel: lowerCamel(cmd.name),
      opcode: cmd.opcode,
      opcodeHex: hex4(cmd.opcode),
      implemented: cmd.implemented,
      recordBytes: record,
      fields,
      index,
    };
  });

  for (const c of ast.consts) {
    if (c.value < 0) {
      throw new ZidlError(`const '${c.name}' must be non-negative`, { line: 1, col: 1 });
    }
  }

  const partial = {
    abi, consts: ast.consts, enums: ast.enums,
    structs: structIR as ReadonlyMap<string, StructIR>, commands,
  };
  return { ...partial, identityText: identityText(partial) };
}
