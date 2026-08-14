// parser.ts — recursive-descent parser for .zidl (capture_format.md 1).
// Accepts the ratified grammar incl. the command status keyword
// (implemented | reserved, mandatory) — the ABI contract is explicit.

import { Token, tokenize } from './lexer.js';
import {
  AbiDecl, CommandDecl, ConstDecl, EnumDecl, EnumEntryAst, FieldAst,
  PRIM_TYPES, SourcePos, StructDecl, ZidlAst, ZidlError,
} from './types.js';

const CONST_TYPES = new Set(['u8', 'u16', 'u32', 'u64']);

export class Parser {
  private idx = 0;

  constructor(private readonly tokens: readonly Token[]) {}

  private peek(): Token {
    return this.tokens[this.idx]!;
  }

  private next(): Token {
    return this.tokens[this.idx++]!;
  }

  private expect(kind: Token['kind'], what: string): Token {
    const t = this.peek();
    if (t.kind !== kind) {
      throw new ZidlError(`expected ${what}, got '${t.text}'`, t.pos);
    }
    return this.next();
  }

  private expectIdent(what: string): string {
    return this.expect('ident', what).text;
  }

  private expectInt(what: string): number {
    return this.expect('int', what).value;
  }

  parse(): ZidlAst {
    const abi = this.parseAbi();
    const consts: ConstDecl[] = [];
    const enums: EnumDecl[] = [];
    const structs: StructDecl[] = [];
    const commands: CommandDecl[] = [];

    while (this.peek().kind !== 'eof') {
      const kw = this.expectIdent('declaration (const/enum/struct/command)');
      switch (kw) {
        case 'const': consts.push(this.parseConst()); break;
        case 'enum': enums.push(this.parseEnum()); break;
        case 'struct': structs.push(this.parseStruct()); break;
        case 'command': commands.push(this.parseCommand()); break;
        default:
          throw new ZidlError(
            `unknown top-level declaration '${kw}' (expected const/enum/struct/command)`,
            this.peek().pos,
          );
      }
    }

    if (commands.length === 0) {
      throw new ZidlError('no commands declared', { line: 1, col: 1 });
    }
    return { abi, consts, enums, structs, commands };
  }

  private parseAbi(): AbiDecl {
    const first = this.expectIdent("'abi'");
    if (first !== 'abi') {
      throw new ZidlError(`file must start with an abi declaration, got '${first}'`,
        this.peek().pos);
    }
    const name = this.expectIdent('abi name');
    this.expect('lbrace', "'{'");
    const decl: AbiDecl = {
      name, version: -1, endian: 'little', commandAlignment: -1, opcodeWidth: 'u16',
    };
    let sawVersion = false, sawEndian = false, sawAlign = false, sawWidth = false;
    while (this.peek().kind !== 'rbrace') {
      const attr = this.expectIdent('abi attribute (version/endian/command_alignment/opcode_width)');
      switch (attr) {
        case 'version':
          decl.version = this.expectInt('version number');
          sawVersion = true; break;
        case 'endian': {
          const v = this.expectIdent("'little' | 'big'");
          if (v !== 'little' && v !== 'big') {
            throw new ZidlError(`endian must be little|big, got '${v}'`, this.peek().pos);
          }
          decl.endian = v; sawEndian = true; break;
        }
        case 'command_alignment':
          decl.commandAlignment = this.expectInt('command_alignment');
          sawAlign = true; break;
        case 'opcode_width': {
          const v = this.expectIdent("'u8' | 'u16'");
          if (v !== 'u8' && v !== 'u16') {
            throw new ZidlError(`opcode_width must be u8|u16, got '${v}'`, this.peek().pos);
          }
          decl.opcodeWidth = v; sawWidth = true; break;
        }
        default:
          throw new ZidlError(`unknown abi attribute '${attr}'`, this.peek().pos);
      }
    }
    this.expect('rbrace', "'}'");
    for (const [saw, what] of [[sawVersion, 'version'], [sawEndian, 'endian'],
      [sawAlign, 'command_alignment'], [sawWidth, 'opcode_width']] as const) {
      if (!saw) {
        throw new ZidlError(`abi declaration is missing '${what}'`, this.peek().pos);
      }
    }
    return decl;
  }

  private parseConst(): ConstDecl {
    const type = this.expectIdent('const type') as ConstDecl['type'];
    if (!CONST_TYPES.has(type)) {
      throw new ZidlError(`const type must be one of u8/u16/u32/u64, got '${type}'`, this.peek().pos);
    }
    const name = this.expectIdent('const name');
    this.expect('equals', "'='");
    const value = this.expectInt('const value');
    this.expect('semicolon', "';'");
    return { type, name, value };
  }

  private parseEnum(): EnumDecl {
    const name = this.expectIdent('enum name');
    let type = 'u32';
    if (this.peek().kind === 'colon') {
      this.next();
      type = this.expectIdent('enum backing type');
      if (!(type in PRIM_TYPES) || type === 'pad') {
        throw new ZidlError(`enum backing type must be a primitive, got '${type}'`, this.peek().pos);
      }
    }
    this.expect('lbrace', "'{'");
    const entries: EnumEntryAst[] = [];
    while (this.peek().kind !== 'rbrace') {
      const ename = this.expectIdent('enum entry name');
      this.expect('equals', "'='");
      const value = this.expectInt('enum entry value');
      this.expect('semicolon', "';'");
      entries.push({ name: ename, value });
    }
    this.expect('rbrace', "'}'");
    if (entries.length === 0) {
      throw new ZidlError(`enum '${name}' has no entries`, this.peek().pos);
    }
    return { name, type, entries };
  }

  private parseStruct(): StructDecl {
    const name = this.expectIdent('struct name');
    this.expect('lbrace', "'{'");
    const fields: FieldAst[] = [];
    while (this.peek().kind !== 'rbrace') {
      fields.push(this.parseField());
    }
    this.expect('rbrace', "'}'");
    return { name, fields };
  }

  private parseCommand(): CommandDecl {
    const name = this.expectIdent('command name');
    const opcode = this.expectInt('opcode (hex, e.g. 0x0010)').valueOf();
    const statusTok = this.expectIdent('command status (implemented | reserved)');
    if (statusTok !== 'implemented' && statusTok !== 'reserved') {
      throw new ZidlError(
        `command status must be 'implemented' or 'reserved' (mandatory), got '${statusTok}'`,
        this.peek().pos,
      );
    }
    this.expect('lbrace', "'{'");
    const fields: FieldAst[] = [];
    while (this.peek().kind !== 'rbrace') {
      fields.push(this.parseField());
    }
    this.expect('rbrace', "'}'");
    return { name, opcode, implemented: statusTok === 'implemented', fields, pos: this.peek().pos };
  }

  private parseField(): FieldAst {
    const pos = this.peek().pos;
    const type = this.expectIdent('field type');
    let handleKind: string | undefined;

    // handle32[kind]?
    if (type === 'handle32') {
      if (this.peek().kind === 'lbracket') {
        this.next();
        handleKind = this.expectIdent('handle kind');
        this.expect('rbracket', "']'");
      }
    }

    // pads are anonymous: `pad[12];` (P5 grammar) — layout uniquifies names
    const anonymousPad = type === 'pad' &&
      (this.peek().kind === 'lbracket' || this.peek().kind === 'semicolon');
    const name = anonymousPad ? 'pad' : this.expectIdent('field name');

    let count = 1;
    if (this.peek().kind === 'lbracket') {
      this.next();
      count = this.expectInt('array length');
      this.expect('rbracket', "']'");
      if (count <= 0) {
        throw new ZidlError(`array length must be >= 1, got ${count}`, pos);
      }
    }
    this.expect('semicolon', "';'");
    return { name, type, count, isPad: type === 'pad', handleKind, pos };
  }
}
