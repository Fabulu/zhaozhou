// emit_sv.ts — fpga/rtl/generated/zhao_abi_pkg.sv emitter (P5 1.5).
//
// THE byte-identity hazard (plan R1): SystemVerilog packed structs pack
// MSB-first, so the FIRST .zidl field (lowest wire offset) must be declared
// LAST. Every struct below is emitted in reverse field order, and the
// pack/unpack functions address each field through explicit bit ranges
// (v[OFF*8 +: WIDTH]) computed from the LayoutIR — if declaration order ever
// drifts from the layout, the golden round-trip catches it immediately.
//
// Conservative subset (charter 2): package, packed structs/enums, functions,
// bounded loops. No classes/queues/interfaces.

import { CRC32C_TABLE } from './crc32c.js';
import { CommandIR, FieldIR, LayoutIR, StructIR, snakeCase, upperSnake } from './types.js';

const svU = (bits: number) => `logic [${bits - 1}:0]`;

/** .zidl field names that collide with SystemVerilog keywords get _f. */
const SV_KEYWORDS = new Set([
  'alias', 'always', 'assert', 'assign', 'begin', 'bit', 'byte', 'case', 'cell',
  'class', 'clocking', 'config', 'deassign', 'default', 'defparam', 'disable',
  'edge', 'else', 'end', 'enum', 'event', 'final', 'for', 'force', 'foreach',
  'fork', 'function', 'generate', 'genvar', 'if', 'initial', 'inout', 'input',
  'interface', 'join', 'localparam', 'logic', 'module', 'nand', 'negedge',
  'nor', 'not', 'or', 'output', 'package', 'parameter', 'pmos', 'posedge',
  'primitive', 'program', 'property', 'rand', 'real', 'ref', 'reg', 'release',
  'repeat', 'return', 'sequence', 'specify', 'struct', 'supply0', 'supply1',
  'table', 'time', 'tri', 'type', 'unsigned', 'use', 'var', 'wait', 'while',
  'wire',
]);
const svId = (name: string): string => (SV_KEYWORDS.has(name) ? `${name}_f` : name);

/** wire type of a leaf/field in SV bits */
function fieldBits(f: FieldIR): number {
  if (f.kind === 'handle') return 32;
  if (f.kind === 'struct') {
    return -1; // handled by caller (typed member)
  }
  switch (f.type) {
    case 'u8': case 'i8': case 'pad': return 8;
    case 'u16': case 'i16': return 16;
    case 'u32': case 'i32': case 'fx16': return 32;
    case 'u64': case 'i64': case 'fx32': return 64;
    default: return 32;
  }
}

function svComment(t: string): string {
  return t === 'fx16' ? 'fx16 = Q16.16 in 32 bits (qformats.md)'
    : t === 'fx32' ? 'fx32 = Q32.32 in 64 bits' : t;
}

/** emit a packed struct body in REVERSE field order (MSB-first packing). */
function structBody(fields: readonly FieldIR[], offsetBase: number): string[] {
  const out: string[] = [];
  const rev = [...fields].reverse();
  for (const f of rev) {
    if (f.kind === 'pad') {
      out.push(`    ${svU(f.count * 8)} ${svId(f.name)};  // ${f.count} zero byte(s) @${offsetBase + f.offset}`);
      continue;
    }
    if (f.kind === 'struct') {
      const t = `zhao_${snakeCase(f.type)}_t`;
      if (f.count > 1) {
        // fixed arrays are flattened into indexed members
        for (let k = f.count - 1; k >= 0; k--) {
          out.push(`    ${t} ${svId(f.name)}_${k};  // @${offsetBase + f.offset + k * (f.size / f.count)}`);
        }
      } else {
        out.push(`    ${t} ${svId(f.name)};  // ${f.size} B @${offsetBase + f.offset}`);
      }
      continue;
    }
    const bits = fieldBits(f);
    const suffix = f.kind === 'handle' ? '  // handle32 {index:24, generation:8}' : '';
    if (f.count > 1) {
      for (let k = f.count - 1; k >= 0; k--) {
        out.push(`    ${svU(bits)} ${svId(f.name)}_${k};  // ${svComment(f.type)} @${offsetBase + f.offset + k * (f.size / f.count)}${suffix}`);
      }
    } else {
      out.push(`    ${svU(bits)} ${svId(f.name)};  // ${svComment(f.type)} @${offsetBase + f.offset}${suffix}`);
    }
  }
  return out;
}

/** member access for a field (arrays flattened) */
function memberExpr(f: FieldIR, k: number): string {
  return f.count > 1 ? `${svId(f.name)}_${k}` : svId(f.name);
}

/** (name, offset, width) of every leaf MEMBER of a struct/record field list */
interface MemberRef {
  readonly expr: string; // member path prefix
  readonly offset: number; // byte offset within the unit
  readonly bits: number; // member width in bits (0 = whole-field/pad copy)
  readonly structType?: string;
  readonly structSize?: number; // pad byte count when bits === 0
  readonly offsetName?: string; // localparam name for the byte offset, if any
}

function memberRefs(
  fields: readonly FieldIR[],
  structs: ReadonlyMap<string, StructIR>,
  offsetBase: number,
  /** optional: localparam NAME for a field's (element k) byte offset */
  offName?: (f: FieldIR, k: number) => string,
): MemberRef[] {
  const out: MemberRef[] = [];
  for (const f of fields) {
    // struct-typed fields: ONE whole-member copy (packed struct <-> slice)
    if (f.kind === 'struct') {
      const elem = f.size / f.count;
      for (let k = 0; k < f.count; k++) {
        out.push({
          expr: memberExpr(f, k),
          offset: offsetBase + f.offset + k * elem,
          bits: 0, // whole-field copy, width from structSize
          structType: f.type,
          structSize: elem,
          offsetName: offName?.(f, k),
        });
      }
      continue;
    }
    // pads: ONE member covering the whole field (count*8 bits)
    if (f.kind === 'pad') {
      out.push({
        expr: svId(f.name),
        offset: offsetBase + f.offset,
        bits: 0, // whole-field copy, width from structSize
        structSize: f.count,
        offsetName: offName?.(f, 0),
      });
      continue;
    }
    const bits = fieldBits(f);
    for (let k = 0; k < f.count; k++) {
      out.push({
        expr: memberExpr(f, k),
        offset: offsetBase + f.offset + k * (f.size / f.count),
        bits,
        offsetName: offName?.(f, k),
      });
    }
  }
  return out;
}

export function emitSv(ir: LayoutIR): string {
  const L: string[] = [];
  const P = (s: string) => L.push(s);
  const structs = ir.structs;

  P('// GENERATED FILE - DO NOT EDIT');
  P('// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).');
  P('// Law: spec/capture_format.md.');
  P('//');
  P('// BYTE-IDENTITY HAZARD (plan R1): packed structs are declared in REVERSE');
  P('// field order (SV packs MSB-first, the wire is little-endian LSB-first).');
  P('// pack/unpack address fields via explicit bit ranges from this layout —');
  P('// golden vectors (tests/abi/golden) are the enforcement, not reasoning.');
  P('');
  P('package zhao_abi_pkg;');
  P('');
  P('  // ---------------------------------------------------------- ABI ---');
  P(`  localparam int unsigned ZHAO_ABI_VERSION        = ${ir.abi.version};`);
  P(`  localparam int unsigned ZHAO_COMMAND_ALIGNMENT = ${ir.abi.commandAlignment};`);
  P('  // exported ABI constants: consumed by importing modules (stub top, probe),');
  P('  // not necessarily referenced inside this package.');
  P('  /* verilator lint_off UNUSEDPARAM */');
  for (const c of ir.consts) {
    P(`  localparam logic [31:0] ${c.name} = 32'd${c.value};`);
  }
  P('  /* verilator lint_on UNUSEDPARAM */');
  P('');
  P('  // frame packet (capture_format.md 3)');
  P("  localparam logic [31:0] ZHAO_FRAME_MAGIC        = 32'h314B505A;  // 'Z','P','K','1' LE");
  P('  localparam int unsigned ZHAO_FRAME_HEADER_BYTES = 36;');
  P('  localparam int unsigned ZHAO_FRAME_OVERHEAD     = 40;');
  P("  localparam logic [15:0] ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 16'h0001;");
  P('  /* verilator lint_off UNUSEDPARAM */');
  P("  localparam logic [7:0]  ZHAO_COMPL_DONE = 8'h01;  // status-output bits (stub top)");
  P("  localparam logic [7:0]  ZHAO_COMPL_ERR  = 8'h02;");
  P('  /* verilator lint_on UNUSEDPARAM */');
  for (const [nm, off] of [['MAGIC', 0], ['ABI_VERSION', 4], ['FLAGS', 6], ['FRAME_ID', 8],
    ['SEQUENCE', 12], ['RESOURCE_EPOCH', 16], ['DEADLINE', 20], ['COMMAND_COUNT', 24],
    ['COMMAND_BYTES', 28], ['HEADER_CRC', 32]] as const) {
    P(`  localparam int unsigned ZHAO_OFF_${nm} = ${off};`);
  }
  P('');
  P('  // error codes — shared verbatim across C++/TS/SV (8-bit codes in v1)');
  P('  typedef enum logic [7:0] {');
  const errEnum = ir.enums.find((e) => e.name === 'zhao_abi_error');
  if (errEnum) {
    errEnum.entries.forEach((e, i) => {
      const comma = i === errEnum!.entries.length - 1 ? '' : ',';
      P(`    ${e.name} = 8'd${e.value}${comma}`);
    });
  }
  P('  } zhao_abi_error_e;');
  P('');
  P('  // opcodes');
  for (const c of ir.commands) {
    P(`  localparam logic [15:0] ZHAO_OP_${upperSnake(c.name)} = 16'h${c.opcode.toString(16).toUpperCase().padStart(4, '0')};`);
  }
  P("  localparam logic [15:0] ZHAO_DEBUG_OPCODE_LO = 16'hF000;");
  P("  localparam logic [15:0] ZHAO_DEBUG_OPCODE_HI = 16'hF0FF;");
  P('');

  // ---- CRC-32C -----------------------------------------------------------------
  P('  // CRC-32C (Castagnoli), capture_format.md 2: poly 0x82F63B78 reflected,');
  P('  // init/xorout 0xFFFFFFFF. Same table as the C++/TS modules (goldens lock).');
  P('  localparam logic [31:0] ZHAO_CRC32C_TABLE [0:255] = \'{');
  for (let i = 0; i < 256; i += 4) {
    P(`    ${[0, 1, 2, 3].map((k) => `32'h${(CRC32C_TABLE[i + k]!).toString(16).toUpperCase().padStart(8, '0')}`).join(', ')}${i + 3 === 255 ? '' : ','}`);
  }
  P('  };');
  P('');
  P('  // per-byte step (capture_format.md 2.2) — synthesizable alternative form');
  P('  function automatic logic [31:0] zhao_crc32c_step(input logic [31:0] c,');
  P('                                                  input logic [7:0]  d);');
  P('    logic [31:0] crc;');
  P('    begin');
  P('      crc = c ^ {24\'b0, d};');
  P("      for (int i = 0; i < 8; i++)");
  P("        crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);");
  P('      zhao_crc32c_step = crc;');
  P('    end');
  P('  endfunction');
  P('');
  P('  // finalized CRC over n bytes of p starting at off (running form of the spec)');
  P('  function automatic logic [31:0] zhao_crc32c_bytes(input logic [7:0] p [],');
  P('                                                   input int unsigned off,');
  P('                                                   input int unsigned n);');
  P('    logic [31:0] c;');
  P('    begin');
  P("      c = 32'hFFFFFFFF;");
  P('      for (int unsigned i = 0; i < n; i++)');
  P('        c = ZHAO_CRC32C_TABLE[c[7:0] ^ p[off+i]] ^ (c >> 8);');
  P('      zhao_crc32c_bytes = ~c;');
  P('    end');
  P('  endfunction');
  P('');

  // ---- byte-get helpers ----------------------------------------------------------
  P('  function automatic logic [15:0] zhao_get16(input logic [7:0] p [],');
  P('                                            input int unsigned off);');
  P('    zhao_get16 = {p[off+1], p[off]};');
  P('  endfunction');
  P('');
  P('  function automatic logic [31:0] zhao_get32(input logic [7:0] p [],');
  P('                                            input int unsigned off);');
  P('    zhao_get32 = {p[off+3], p[off+2], p[off+1], p[off]};');
  P('  endfunction');
  P('');

  // ---- composed structs (reverse field order!) -------------------------------------
  for (const s of structs.values()) {
    P(`  // ${s.name}: ${s.size} B (spec/commands.zidl). REVERSE field order.`);
    P(`  typedef struct packed {`);
    for (const line of structBody(s.fields, 0)) P(line);
    P(`  } zhao_${snakeCase(s.name)}_t;`);
    P('');
  }

  // ---- per-command record structs + offsets ------------------------------------------
  for (const c of ir.commands) {
    const sn = snakeCase(c.name);
    P(`  // ${c.name} ${c.opcodeHex}: ${c.recordBytes}-B record (${c.implemented ? 'implemented' : 'reserved'}).`);
    P(`  // Command header fields first on the wire, then payload; declared reversed.`);
    P(`  typedef struct packed {`);
    // synthetic header members get an h_ prefix so payload fields may reuse
    // the same names (BeginFrame/SetView both declare a payload 'flags')
    const fields = [
      { name: 'h_reserved0', kind: 'scalar', type: 'u32', count: 1, offset: 12, size: 4, leaves: [], pos: { line: 0, col: 0 } } as FieldIR,
      { name: 'h_flags', kind: 'scalar', type: 'u32', count: 1, offset: 8, size: 4, leaves: [], pos: { line: 0, col: 0 } } as FieldIR,
      { name: 'h_source_id', kind: 'scalar', type: 'u32', count: 1, offset: 4, size: 4, leaves: [], pos: { line: 0, col: 0 } } as FieldIR,
      { name: 'h_record_bytes', kind: 'scalar', type: 'u16', count: 1, offset: 2, size: 2, leaves: [], pos: { line: 0, col: 0 } } as FieldIR,
      { name: 'h_opcode', kind: 'scalar', type: 'u16', count: 1, offset: 0, size: 2, leaves: [], pos: { line: 0, col: 0 } } as FieldIR,
      ...c.fields.map((f) => ({ ...f, offset: 16 + f.offset })),
    ];
    for (const line of structBody(fields, 0)) P(line);
    P(`  } zhao_rec_${sn}_t;`);
    P('');
    // record-relative offsets for pack/unpack (payload fields + header);
    // the BYTES constant is a package export (consumers may reference it)
    P('  /* verilator lint_off UNUSEDPARAM */');
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_BYTES = ${c.recordBytes};`);
    P('  /* verilator lint_on UNUSEDPARAM */');
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_H_OPCODE = 0;`);
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_H_RECORD_BYTES = 2;`);
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_H_SOURCE_ID = 4;`);
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_H_FLAGS = 8;`);
    P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_H_RESERVED0 = 12;`);
    for (const f of c.fields) {
      if (f.kind === 'struct') {
        const elem = f.size / f.count;
        for (let k = 0; k < f.count; k++) {
          P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_${f.name.toUpperCase()}${f.count > 1 ? `_${k}` : ''} = ${16 + f.offset + k * elem};`);
        }
        continue;
      }
      const per = f.kind === 'pad' ? 1 : f.size / f.count;
      for (let k = 0; k < (f.kind === 'pad' ? 1 : f.count); k++) {
        const nm = f.kind === 'pad' ? f.name.toUpperCase() : `${f.name.toUpperCase()}${f.count > 1 ? `_${k}` : ''}`;
        P(`  localparam int unsigned ZHAO_${upperSnake(c.name)}_OFF_${nm} = ${16 + f.offset + k * per};`);
      }
    }
    P('');
  }

  // ---- pack/unpack per struct --------------------------------------------------------
  for (const s of structs.values()) {
    const sn = snakeCase(s.name);
    const total = s.size * 8;
    P(`  function automatic logic [${total - 1}:0] zhao_pack_${sn}(input zhao_${sn}_t c);`);
    P(`    logic [${total - 1}:0] v;`);
    P('    begin');
    for (const r of memberRefs(s.fields, structs, 0)) {
      if (r.bits === 0) {
        P(`      v[${r.offset * 8} +: ${r.structSize! * 8}] = c.${r.expr};`);
      } else {
        P(`      v[${r.offset * 8} +: ${r.bits}] = c.${r.expr};`);
      }
    }
    P(`      zhao_pack_${sn} = v;`);
    P('    end');
    P('  endfunction');
    P('');
    P(`  function automatic zhao_${sn}_t zhao_unpack_${sn}(input logic [${total - 1}:0] v);`);
    P(`    zhao_${sn}_t c;`);
    P('    begin');
    for (const r of memberRefs(s.fields, structs, 0)) {
      if (r.bits === 0) {
        P(`      c.${r.expr} = v[${r.offset * 8} +: ${r.structSize! * 8}];`);
      } else {
        P(`      c.${r.expr} = v[${r.offset * 8} +: ${r.bits}];`);
      }
    }
    P(`      zhao_unpack_${sn} = c;`);
    P('    end');
    P('  endfunction');
    P('');
  }

  // ---- pack/unpack per command record ---------------------------------------------------
  for (const c of ir.commands) {
    const sn = snakeCase(c.name);
    const total = c.recordBytes * 8;
    const hdrFields: FieldIR[] = [
      { name: 'h_opcode', kind: 'scalar', type: 'u16', count: 1, offset: 0, size: 2, leaves: [], pos: { line: 0, col: 0 } },
      { name: 'h_record_bytes', kind: 'scalar', type: 'u16', count: 1, offset: 2, size: 2, leaves: [], pos: { line: 0, col: 0 } },
      { name: 'h_source_id', kind: 'scalar', type: 'u32', count: 1, offset: 4, size: 4, leaves: [], pos: { line: 0, col: 0 } },
      { name: 'h_flags', kind: 'scalar', type: 'u32', count: 1, offset: 8, size: 4, leaves: [], pos: { line: 0, col: 0 } },
      { name: 'h_reserved0', kind: 'scalar', type: 'u32', count: 1, offset: 12, size: 4, leaves: [], pos: { line: 0, col: 0 } },
    ];
    const cmd = c;
    const nameFor = (f: FieldIR, k: number): string => {
      const base = f.name.startsWith('h_')
        ? `H_${f.name.slice(2).toUpperCase()}`
        : `${f.name.toUpperCase()}${f.count > 1 && f.kind !== 'pad' ? `_${k}` : ''}`;
      return `ZHAO_${upperSnake(cmd.name)}_OFF_${base}`;
    };
    const refs = [
      ...memberRefs(hdrFields, structs, 0, nameFor),
      ...memberRefs(c.fields, structs, 16, nameFor),
    ];
    P(`  function automatic logic [${total - 1}:0] zhao_pack_${sn}(input zhao_rec_${sn}_t c);`);
    P(`    logic [${total - 1}:0] v;`);
    P('    begin');
    for (const r of refs) {
      const lo = r.offsetName ? `${r.offsetName}*8` : `${r.offset * 8}`;
      if (r.bits === 0) {
        P(`      v[${lo} +: ${r.structSize! * 8}] = c.${r.expr};`);
      } else {
        P(`      v[${lo} +: ${r.bits}] = c.${r.expr};`);
      }
    }
    P(`      zhao_pack_${sn} = v;`);
    P('    end');
    P('  endfunction');
    P('');
    P(`  function automatic zhao_rec_${sn}_t zhao_unpack_${sn}(input logic [${total - 1}:0] v);`);
    P(`    zhao_rec_${sn}_t c;`);
    P('    begin');
    for (const r of refs) {
      const lo = r.offsetName ? `${r.offsetName}*8` : `${r.offset * 8}`;
      if (r.bits === 0) {
        P(`      c.${r.expr} = v[${lo} +: ${r.structSize! * 8}];`);
      } else {
        P(`      c.${r.expr} = v[${lo} +: ${r.bits}];`);
      }
    }
    P(`      zhao_unpack_${sn} = c;`);
    P('    end');
    P('  endfunction');
    P('');
  }

  // ---- opcode -> record bytes ---------------------------------------------------------
  P('  // 0 = unknown opcode (capture_format.md 3.2 step 5)');
  P('  function automatic int unsigned zhao_opcode_record_bytes(input logic [15:0] op);');
  P('    begin');
  P('      case (op)');
  for (const c of ir.commands) {
    P(`        ZHAO_OP_${upperSnake(c.name)}: zhao_opcode_record_bytes = ${c.recordBytes};`);
  }
  P('        default: zhao_opcode_record_bytes = 0;');
  P('      endcase');
  P('    end');
  P('  endfunction');
  P('');

  // ---- per-command pad-zero helper ------------------------------------------------------
  P('  // true if any byte in p[base+off .. base+off+n) is nonzero');
  P('  function automatic logic zhao_bytes_nonzero(input logic [7:0] p [],');
  P('                                             input int unsigned base,');
  P('                                             input int unsigned off,');
  P('                                             input int unsigned n);');
  P('    begin');
  P('      zhao_bytes_nonzero = 1\'b0;');
  P('      for (int unsigned i = 0; i < n; i++)');
  P('        if (p[base+off+i] != 8\'h00) zhao_bytes_nonzero = 1\'b1;');
  P('    end');
  P('  endfunction');
  P('');
  P('  // true if any declared pad byte of the record at stream offset off is nonzero');
  P('  function automatic logic zhao_record_pad_nonzero(input logic [15:0] op,');
  P('                                                  input logic [7:0] p [],');
  P('                                                  input int unsigned base);');
  P('    begin');
  P('      zhao_record_pad_nonzero = 1\'b0;');
  P('      case (op)');
  for (const c of ir.commands) {
    const pads = c.fields.filter((f) => f.kind === 'pad');
    if (pads.length === 0) continue;
    P(`        ZHAO_OP_${upperSnake(c.name)}: begin`);
    for (const f of pads) {
      P(`          if (zhao_bytes_nonzero(p, base, ${16 + f.offset}, ${f.count})) zhao_record_pad_nonzero = 1'b1;`);
    }
    P('        end');
  }
  P('        default: zhao_record_pad_nonzero = 1\'b0;');
  P('      endcase');
  P('    end');
  P('  endfunction');
  P('');

  // ---- frame header struct + pack/unpack --------------------------------------------------
  P('  // sealed frame header (capture_format.md 3), REVERSE field order');
  P('  typedef struct packed {');
  P('    logic [31:0] header_crc;');
  P('    logic [31:0] command_bytes;');
  P('    logic [31:0] command_count;');
  P('    logic [31:0] deadline_cycles;');
  P('    logic [31:0] resource_epoch;');
  P("    logic [31:0] sequence_f;  // .zidl 'sequence' (SV keyword)");
  P('    logic [31:0] frame_id;');
  P('    logic [15:0] flags;');
  P('    logic [15:0] abi_version;');
  P('    logic [31:0] magic;');
  P('  } zhao_frame_hdr_t;');
  P('');
  P('  function automatic logic [287:0] zhao_pack_frame_hdr(input zhao_frame_hdr_t c);');
  P('    logic [287:0] v;');
  P('    begin');
  P('      v[ZHAO_OFF_MAGIC*8         +: 32] = c.magic;');
  P('      v[ZHAO_OFF_ABI_VERSION*8   +: 16] = c.abi_version;');
  P('      v[ZHAO_OFF_FLAGS*8         +: 16] = c.flags;');
  P('      v[ZHAO_OFF_FRAME_ID*8      +: 32] = c.frame_id;');
  P('      v[ZHAO_OFF_SEQUENCE*8      +: 32] = c.sequence_f;');
  P('      v[ZHAO_OFF_RESOURCE_EPOCH*8+: 32] = c.resource_epoch;');
  P('      v[ZHAO_OFF_DEADLINE*8      +: 32] = c.deadline_cycles;');
  P('      v[ZHAO_OFF_COMMAND_COUNT*8 +: 32] = c.command_count;');
  P('      v[ZHAO_OFF_COMMAND_BYTES*8 +: 32] = c.command_bytes;');
  P('      v[ZHAO_OFF_HEADER_CRC*8    +: 32] = c.header_crc;');
  P('      zhao_pack_frame_hdr = v;');
  P('    end');
  P('  endfunction');
  P('');
  P('  function automatic zhao_frame_hdr_t zhao_unpack_frame_hdr(input logic [7:0] p []);');
  P('    zhao_frame_hdr_t c;');
  P('    begin');
  P('      c.magic          = zhao_get32(p, ZHAO_OFF_MAGIC);');
  P('      c.abi_version    = zhao_get16(p, ZHAO_OFF_ABI_VERSION);');
  P('      c.flags          = zhao_get16(p, ZHAO_OFF_FLAGS);');
  P('      c.frame_id       = zhao_get32(p, ZHAO_OFF_FRAME_ID);');
  P('      c.sequence_f     = zhao_get32(p, ZHAO_OFF_SEQUENCE);');
  P('      c.resource_epoch = zhao_get32(p, ZHAO_OFF_RESOURCE_EPOCH);');
  P('      c.deadline_cycles= zhao_get32(p, ZHAO_OFF_DEADLINE);');
  P('      c.command_count  = zhao_get32(p, ZHAO_OFF_COMMAND_COUNT);');
  P('      c.command_bytes  = zhao_get32(p, ZHAO_OFF_COMMAND_BYTES);');
  P('      c.header_crc     = zhao_get32(p, ZHAO_OFF_HEADER_CRC);');
  P('      zhao_unpack_frame_hdr = c;');
  P('    end');
  P('  endfunction');
  P('');

  // ---- layout self-check -----------------------------------------------------------------
  P('  // $bits sanity for every generated struct (probe asserts this at reset).');
  P('  function automatic logic zhao_layout_ok();');
  P('    begin');
  P('      zhao_layout_ok = 1\'b1;');
  for (const c of ir.commands) {
    P(`      if ($bits(zhao_rec_${snakeCase(c.name)}_t) != 8*${c.recordBytes}) zhao_layout_ok = 1'b0;`);
  }
  for (const s of structs.values()) {
    P(`      if ($bits(zhao_${snakeCase(s.name)}_t) != 8*${s.size}) zhao_layout_ok = 1'b0;`);
  }
  P('    end');
  P('  endfunction');
  P('');

  // ---- frame validator (normative order, capture_format.md 3.2) ---------------------------
  P('  // Fail-safe validation order — byte-for-byte the same contract as the C++');
  P('  // (zref_frame) and TS (frame.ts) validators. On any error, no partial state.');
  P('  function automatic zhao_abi_error_e zhao_frame_validate(');
  P('      input logic [7:0] pkt [],');
  P('      input int unsigned len,');
  P('      input int unsigned slot_bytes,');
  P('      output int unsigned commands_consumed');
  P('  );');
  P('    logic [31:0] magic, command_bytes, command_count, hcrc, pcrc, calc;');
  P('    logic [15:0] abi_version;');
  P('    int unsigned off, rec_bytes, seen, want;')
  P('    logic [15:0] opcode;');
  P('    logic any_debug;');
  P('    begin');
  P('      commands_consumed = 0;');
  P('      any_debug = 1\'b0;');
  P('      seen = 0;');
  P('');
  P('      // 1. magic (whenever 4 bytes exist), then header completeness, abi, flags');
  P('      if (len < 4) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      magic = zhao_get32(pkt, ZHAO_OFF_MAGIC);');
  P('      if (magic != ZHAO_FRAME_MAGIC) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_MAGIC;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if (len < ZHAO_FRAME_HEADER_BYTES) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      abi_version = zhao_get16(pkt, ZHAO_OFF_ABI_VERSION);');
  P('      if (abi_version != ZHAO_ABI_VERSION[15:0]) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_ABI_VERSION;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if ((zhao_get16(pkt, ZHAO_OFF_FLAGS) & ~ZHAO_FRAME_FLAG_CONTAINS_DEBUG) != 16\'h0000) begin');
  P('        zhao_frame_validate = ZH_ABI_RESERVED_FLAG;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('');
  P('      // 2. bounds');
  P('      command_bytes = zhao_get32(pkt, ZHAO_OFF_COMMAND_BYTES);');
  P('      command_count = zhao_get32(pkt, ZHAO_OFF_COMMAND_COUNT);');
  P('      if ((command_bytes % ZHAO_COMMAND_ALIGNMENT) != 0) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if ((ZHAO_FRAME_OVERHEAD + command_bytes) > slot_bytes) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if ((command_count * 16) > command_bytes) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if (len != (ZHAO_FRAME_OVERHEAD + command_bytes)) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('');
  P('      // 3. header CRC over bytes [0,32)');
  P('      calc = zhao_crc32c_bytes(pkt, 0, 32);');
  P('      hcrc = zhao_get32(pkt, ZHAO_OFF_HEADER_CRC);');
  P('      if (calc != hcrc) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_HEADER_CRC;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('');
  P('      // 4. payload CRC over the command stream');
  P('      calc = zhao_crc32c_bytes(pkt, ZHAO_FRAME_HEADER_BYTES, command_bytes);');
  P('      pcrc = zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + command_bytes);');
  P('      if (calc != pcrc) begin');
  P('        zhao_frame_validate = ZH_ABI_BAD_PAYLOAD_CRC;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('');
  P('      // 5./6./9./10. record walk');
  P('      off = 0;');
  P('      while (off < command_bytes) begin');
  P('        if ((off + 16) > command_bytes) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_TRUNCATED;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        opcode    = zhao_get16(pkt, ZHAO_FRAME_HEADER_BYTES + off);');
  P("        rec_bytes = {16'b0, zhao_get16(pkt, ZHAO_FRAME_HEADER_BYTES + off + 2)};");
  P('        if ((rec_bytes % ZHAO_COMMAND_ALIGNMENT) != 0 || rec_bytes < 16) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if ((off + rec_bytes) > command_bytes) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        want = zhao_opcode_record_bytes(16\'(opcode));');
  P('        if (want == 0) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_UNKNOWN_OPCODE;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if (rec_bytes != want) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_BAD_LENGTH;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if (zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + off + 8) != 32\'h0) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_RESERVED_FLAG;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if (zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + off + 12) != 32\'h0) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_RESERVED_FIELD;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if (zhao_record_pad_nonzero(16\'(opcode), pkt, ZHAO_FRAME_HEADER_BYTES + off)) begin');
  P('          commands_consumed = seen;');
  P('          zhao_frame_validate = ZH_ABI_RESERVED_FIELD;');
  P('          return zhao_frame_validate;');
  P('        end');
  P('        if ((16\'(opcode) >= ZHAO_DEBUG_OPCODE_LO) && (16\'(opcode) <= ZHAO_DEBUG_OPCODE_HI))');
  P('          any_debug = 1\'b1;');
  P('        off = off + rec_bytes;');
  P('        seen = seen + 1;');
  P('      end');
  P('');
  P('      commands_consumed = seen;');
  P('      if (seen != command_count) begin');
  P('        zhao_frame_validate = ZH_ABI_COUNT_MISMATCH;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      if (any_debug && ((zhao_get16(pkt, ZHAO_OFF_FLAGS) & ZHAO_FRAME_FLAG_CONTAINS_DEBUG) == 16\'h0000)) begin');
  P('        zhao_frame_validate = ZH_ABI_DEBUG_FLAG_REQUIRED;');
  P('        return zhao_frame_validate;');
  P('      end');
  P('      zhao_frame_validate = ZH_ABI_OK;');
  P('    end');
  P('  endfunction');
  P('');

  P('endpackage : zhao_abi_pkg');
  P('');
  return L.join('\n');
}
