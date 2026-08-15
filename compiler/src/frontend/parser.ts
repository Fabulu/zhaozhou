// parser.ts — hand-written recursive-descent + Pratt parser for Form L1
// (W3.2, plan D2; the EBNF of language-semantics.md §2 is this file's
// contract). Diagnostics are collected, never thrown. OUT-list features are
// REFUSED with FORM-E-7xx codes at the exact site, never silently parsed
// (§8). Recovery keeps parsing after an error (skip to ';' / '}' / next
// declaration keyword) so one corpus file can still yield multiple codes.

import {
  Access, ApplyStmt, BadDecl, BadStmt, CapacityExpr, ConstDecl, EmitArg,
  EmitStmt, EnumDecl, EnumMember, Expr, FieldStmt, FnDecl, Footprint,
  GlobalDecl, ImportDecl, ModuleAst, Param, PoolDecl, PresentationDecl,
  PresentationItem, RecordField, RecordLit, ScenarioDecl, ScenarioItem, SoundDecl,
  Stmt, StructDecl, StructField, SystemDecl, TopDecl, TypeExpr, ViewItem,
} from './ast.js';
import { DiagnosticSink } from './diagnostics.js';
import { SOFT_KEYWORDS, Token, tokenize } from './lexer.js';
import { SourceSpan, span } from './span.js';

/** Reserved-forever / domain keywords -> OUT-list refusal code (§1.1, §8). */
const RESERVED_KW_CODE: ReadonlyMap<string, string> = new Map([
  ['macro', 'FORM-E-701'],
  ['class', 'FORM-E-703'],
  ['interface', 'FORM-E-703'],
  ['extern', 'FORM-E-710'],
  ['importc', 'FORM-E-710'],
  ['f32', 'FORM-E-711'],
  ['f64', 'FORM-E-711'],
  ['float', 'FORM-E-711'],
  ['double', 'FORM-E-711'],
  ['string', 'FORM-E-705'],
  ['while', 'FORM-E-708'],
  ['break', 'FORM-E-712'],
  ['continue', 'FORM-E-712'],
  ['build', 'FORM-E-715'],
  ['warp', 'FORM-E-715'],
  ['formation', 'FORM-E-715'],
  ['stamp', 'FORM-E-715'],
]);

/** OUT-list declaration names that are plain identifiers (not keywords). */
const OUT_DECL_CODE: ReadonlyMap<string, string> = new Map([
  ['form', 'FORM-E-700'],
  ['population', 'FORM-E-714'],
  ['surface', 'FORM-E-716'],
  ['spell', 'FORM-E-716'],
  ['terrain_material', 'FORM-E-713'],
  ['layer', 'FORM-E-717'],
  ['after', 'FORM-E-717'],
  ['transparent_group', 'FORM-E-717'],
  ['barrier', 'FORM-E-717'],
]);

const TOP_DECLS = new Set([
  'import', 'const', 'enum', 'struct', 'pool', 'global', 'system', 'fn',
  'presentation', 'scenario', 'sound',
]);

interface ExprOpts {
  allowRange: boolean;   // '..' permitted (for headers only — E-107 otherwise)
  recordLits: boolean;   // `ident {` may start a record literal
}

const DEFAULT_OPTS: ExprOpts = { allowRange: false, recordLits: true };

export class Parser {
  private idx = 0;
  private readonly file: string;

  constructor(
    private readonly tokens: readonly Token[],
    private readonly sink: DiagnosticSink,
    file: string,
  ) {
    this.file = file;
  }

  // -- token plumbing ------------------------------------------------------

  private peek(off = 0): Token {
    return this.tokens[Math.min(this.idx + off, this.tokens.length - 1)]!;
  }

  private next(): Token {
    const t = this.tokens[this.idx]!;
    if (this.idx < this.tokens.length - 1) this.idx++;
    return t;
  }

  private atPunct(text: string, off = 0): boolean {
    const t = this.peek(off);
    return t.kind === 'punct' && t.text === text;
  }

  private atKw(text: string, off = 0): boolean {
    const t = this.peek(off);
    return t.kind === 'kw' && t.text === text;
  }

  private atIdent(text?: string, off = 0): boolean {
    const t = this.peek(off);
    return t.kind === 'ident' && (text === undefined || t.text === text);
  }

  private error(code: string, at: SourceSpan, message: string): void {
    this.sink.error(code, at, message);
  }

  private expectPunct(text: string, what = `'${text}'`): Token {
    const t = this.peek();
    if (t.kind === 'punct' && t.text === text) return this.next();
    this.error('FORM-E-100', t.span, `expected ${what}, found '${t.text}'`);
    throw ParseError;
  }

  private expectKw(text: string): Token {
    const t = this.peek();
    if (t.kind === 'kw' && t.text === text) return this.next();
    this.error('FORM-E-100', t.span, `expected keyword '${text}', found '${t.text}'`);
    throw ParseError;
  }

  /** Hard keywords in name position are E-109; soft lane keywords pass. */
  private expectName(what: string, allowSoftKw = false): { text: string; span: SourceSpan } {
    const t = this.peek();
    if (t.kind === 'ident') return { text: t.text, span: this.next().span };
    if (t.kind === 'kw') {
      if (allowSoftKw && SOFT_KEYWORDS.has(t.text)) return { text: t.text, span: this.next().span };
      this.error('FORM-E-109', t.span, `keyword '${t.text}' cannot be used as ${what}`);
      throw ParseError;
    }
    this.error('FORM-E-100', t.span, `expected ${what}, found '${t.text}'`);
    throw ParseError;
  }

  private expectInt(what: string): { value: bigint; span: SourceSpan } {
    const t = this.peek();
    if (t.kind === 'int' && t.intVal !== undefined) {
      this.next();
      return { value: t.intVal, span: t.span };
    }
    this.error('FORM-E-100', t.span, `expected ${what} (integer), found '${t.text}'`);
    throw ParseError;
  }

  private expectString(what: string): { value: string; span: SourceSpan } {
    const t = this.peek();
    if (t.kind === 'string') {
      this.next();
      return { value: t.text, span: t.span };
    }
    this.error('FORM-E-100', t.span, `expected ${what} (string), found '${t.text}'`);
    throw ParseError;
  }

  /** Skip to after the next ';' or to just before '}' / EOF (recovery). */
  private syncStmt(): void {
    let guard = 0;
    while (guard++ < 4096) {
      const t = this.peek();
      if (t.kind === 'eof') return;
      if (t.kind === 'punct' && t.text === ';') { this.next(); return; }
      if (t.kind === 'punct' && t.text === '}') return;
      if (t.kind === 'kw' && TOP_DECLS.has(t.text)) return;
      this.next();
    }
  }

  /** Skip a balanced `{ ... }` block (already past the opening '{'). */
  private skipBlock(): void {
    let depth = 1;
    while (depth > 0 && this.peek().kind !== 'eof') {
      const t = this.next();
      if (t.kind === 'punct' && t.text === '{') depth++;
      if (t.kind === 'punct' && t.text === '}') depth--;
    }
  }

  // -- module --------------------------------------------------------------

  parseModule(): ModuleAst {
    const first = this.peek();
    if (!(first.kind === 'kw' && first.text === 'module')) {
      this.error('FORM-E-103', first.span,
        `file must start with 'module <name> { ... }', found '${first.text}'`);
      return { kind: 'Module', name: '<bad>', imports: [], decls: [], span: first.span };
    }
    const kw = this.next();
    const nameTok = this.peek();
    const name = nameTok.kind === 'ident' ? this.next().text : '<bad>';
    if (name === '<bad>') {
      this.error('FORM-E-100', nameTok.span, `expected module name, found '${nameTok.text}'`);
    }
    this.expectPunct('{');
    const imports: ImportDecl[] = [];
    const decls: TopDecl[] = [];
    while (true) {
      const t = this.peek();
      if (t.kind === 'eof') {
        this.error('FORM-E-100', t.span, `expected '}' closing module '${name}', found end of file`);
        break;
      }
      if (t.kind === 'punct' && t.text === '}') { this.next(); break; }
      if (t.kind === 'kw' && t.text === 'import') {
        try { imports.push(this.parseImport()); } catch { this.syncStmt(); }
        continue;
      }
      const decl = this.parseTopDecl();
      if (decl) decls.push(decl);
      else if (this.peek() === t) this.next(); // guarantee progress
    }
    // trailing content after the module's '}'
    const trailing = this.peek();
    if (trailing.kind !== 'eof') {
      this.error('FORM-E-103', trailing.span,
        `trailing content after the module's '}' ('${trailing.text}')`);
    }
    return {
      kind: 'Module', name, imports, decls,
      span: { file: this.file, start: kw.span.start, end: this.peek().span.end },
    };
  }

  private parseImport(): ImportDecl {
    const kw = this.expectKw('import');
    const mod = this.expectName('module name');
    const names: string[] = [];
    if (this.atPunct('{')) {
      this.next();
      names.push(this.expectName('imported name').text);
      while (this.atPunct(',')) { this.next(); names.push(this.expectName('imported name').text); }
      this.expectPunct('}');
    }
    this.expectPunct(';');
    return { kind: 'Import', module: mod.text, names, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  // -- top-level declarations ----------------------------------------------

  private parseTopDecl(): TopDecl | null {
    const t = this.peek();

    if (t.kind === 'kw' && TOP_DECLS.has(t.text)) {
      try {
        switch (t.text) {
          case 'const': return this.parseConst();
          case 'enum': return this.parseEnum();
          case 'struct': return this.parseStruct();
          case 'pool': return this.parsePool();
          case 'global': return this.parseGlobal();
          case 'system': return this.parseSystem();
          case 'fn': return this.parseFn();
          case 'presentation': return this.parsePresentation();
          case 'scenario': return this.parseScenario();
          case 'sound': return this.parseSound();
        }
      } catch {
        this.recoverTop();
        return null;
      }
    }

    if (t.kind === 'punct' && t.text === '@') {
      try { return this.parseFieldDecl(); } catch { this.recoverTop(); return null; }
    }

    // OUT-list refusals (§8) — each carries its exact code.
    if (t.kind === 'ident' && OUT_DECL_CODE.has(t.text)) {
      const code = OUT_DECL_CODE.get(t.text)!;
      this.error(code, t.span, `'${t.text}' declarations are refused in L1 (FORM-E-${code.slice(-3)}; language-semantics §8)`);
      this.skipDeclTail();
      return null;
    }
    if (t.kind === 'kw' && RESERVED_KW_CODE.has(t.text)) {
      const code = RESERVED_KW_CODE.get(t.text)!;
      this.error(code, t.span, `'${t.text}' is refused in L1 (${code})`);
      this.skipDeclTail();
      return null;
    }

    this.error('FORM-E-101', t.span,
      `unexpected token '${t.text}' at top level (a declaration keyword is required)`);
    this.next();
    this.skipDeclTail();
    return null;
  }

  /** After a refusal/error: swallow up to and including a ';' or a block. */
  private skipDeclTail(): void {
    let guard = 0;
    while (guard++ < 8192 && this.peek().kind !== 'eof') {
      const t = this.peek();
      if (t.kind === 'kw' && TOP_DECLS.has(t.text)) return;
      if (t.kind === 'punct' && t.text === '@') return;
      if (t.kind === 'punct' && t.text === '}') return;
      this.next();
      if (t.kind === 'punct' && t.text === ';') return;
      if (t.kind === 'punct' && t.text === '{') this.skipBlock();
    }
  }

  private recoverTop(): void {
    this.syncStmt();
    // syncStmt stops before '}' (module close) — good.
  }

  private parseConst(): ConstDecl {
    const kw = this.expectKw('const');
    const name = this.expectName('const name');
    this.expectPunct(':');
    const type = this.parseType();
    this.expectPunct('=');
    const init = this.parseExpr();
    this.expectPunct(';');
    return { kind: 'Const', name: name.text, type, init, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseEnum(): EnumDecl {
    const kw = this.expectKw('enum');
    const name = this.expectName('enum name');
    this.expectPunct('{');
    const members: EnumMember[] = [];
    while (!this.atPunct('}')) {
      const m = this.expectName('enum member');
      let value: bigint | null = null;
      if (this.atPunct('=')) { this.next(); value = this.expectInt('enum value').value; }
      members.push({ name: m.text, value, span: m.span });
      if (this.atPunct(',')) { this.next(); continue; }
      break;
    }
    if (this.atPunct(',')) this.next(); // trailing comma
    this.expectPunct('}');
    return { kind: 'Enum', name: name.text, members, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseStruct(): StructDecl {
    const kw = this.expectKw('struct');
    const name = this.expectName('struct name');
    if (this.atPunct('<')) {
      this.error('FORM-E-702', this.peek().span,
        "generics beyond pool capacity literals are refused in L1 (FORM-E-702; '<T>' syntax)");
      throw ParseError;
    }
    this.expectPunct('{');
    const fields: StructField[] = [];
    while (!this.atPunct('}')) {
      const f = this.expectName('struct field name', true);
      this.expectPunct(':');
      const type = this.parseType();
      this.expectPunct(';');
      fields.push({ name: f.text, type, span: f.span });
    }
    this.expectPunct('}');
    return { kind: 'Struct', name: name.text, fields, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parsePool(): PoolDecl {
    const kw = this.expectKw('pool');
    const name = this.expectName('pool name');
    this.expectPunct(':');
    const structName = this.expectName('pool struct type').text;
    this.expectPunct('[');
    let capacity: CapacityExpr = null;
    if (this.atPunct(']')) {
      // `pool P: S[]` — a capacity-less collection is dynamic growth (FORM §6)
      this.error('FORM-E-719', this.peek().span,
        'a pool without a capacity is dynamic collection growth, refused in L1 (FORM §6)');
    } else if (this.peek().kind === 'int') {
      const capTok = this.expectInt('capacity');
      capacity = { kind: 'int', value: capTok.value, span: capTok.span };
    } else if (this.atIdent()) {
      const n = this.expectName('capacity const name');
      capacity = { kind: 'name', name: n.text, span: n.span };
    } else {
      this.error('FORM-E-800', this.peek().span,
        `pool capacity must be an integer or a u32 const, found '${this.peek().text}'`);
      this.syncStmt();
    }
    this.expectPunct(']');
    this.expectPunct(';');
    return { kind: 'Pool', name: name.text, structName, capacity, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseGlobal(): GlobalDecl {
    const kw = this.expectKw('global');
    const name = this.expectName('global name');
    this.expectPunct(':');
    const type = this.parseType();
    this.expectPunct('=');
    const init = this.parseExpr();
    this.expectPunct(';');
    return { kind: 'Global', name: name.text, type, init, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseFn(): FnDecl {
    const kw = this.expectKw('fn');
    const name = this.expectName('fn name');
    this.expectPunct('(');
    const params: Param[] = [];
    if (!this.atPunct(')')) {
      do {
        const p = this.expectName('parameter name');
        this.expectPunct(':');
        params.push({ name: p.text, type: this.parseType(), span: p.span });
      } while (this.atPunct(',') && this.next());
    }
    this.expectPunct(')');
    this.expectPunct('->');
    const ret = this.parseType();
    this.expectPunct('{');
    const body = this.parseStmts('fn');
    this.expectPunct('}');
    return { kind: 'Fn', name: name.text, params, ret, body, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseSystem(): SystemDecl {
    const kw = this.expectKw('system');
    const name = this.expectName('system name');
    this.expectKw('every');
    const every = this.expectInt('system rate').value;
    if (!this.atKw('tick') && !this.atKw('ticks')) {
      this.error('FORM-E-100', this.peek().span, `expected 'tick'/'ticks' after the rate, found '${this.peek().text}'`);
      throw ParseError;
    }
    this.next();
    let staggerRate: bigint | null = null;
    let staggerPool: string | null = null;
    let staggerSpan: SourceSpan | null = null;
    if (this.atKw('stagger')) {
      staggerSpan = this.next().span;
      if (this.peek().kind === 'int') staggerRate = this.expectInt('stagger rate').value;
      this.expectKw('over');
      staggerPool = this.expectName('stagger pool').text;
    }
    this.expectKw('reads');
    const reads = this.parseAccessList('writes');
    this.expectKw('writes');
    const writes = this.parseAccessList('{');
    this.expectPunct('{');
    const body = this.parseStmts('system');
    this.expectPunct('}');
    return {
      kind: 'System', name: name.text, every, staggerRate, staggerPool, staggerSpan,
      reads, writes, body, span: join(kw.span, this.tokens[this.idx - 1]!.span),
    };
  }

  private parseAccessList(stopKw: string): Access[] {
    const list: Access[] = [];
    // empty list permitted (spec-issue note): stop at the next keyword or '{'
    if (this.atKw(stopKw) || (stopKw === '{' && this.atPunct('{'))) return list;
    while (true) {
      const t = this.peek();
      // component names are idents, soft keywords (input), or 'terrain'
      let first: string;
      if (t.kind === 'ident' || (t.kind === 'kw' && SOFT_KEYWORDS.has(t.text))) {
        first = t.text;
      } else {
        this.error('FORM-E-100', t.span, `expected an access name, found '${t.text}'`);
        throw ParseError;
      }
      const start = this.next().span;
      const parts = [first];
      while (this.atPunct('.')) {
        this.next();
        const part = this.expectName('component field', true);
        parts.push(part.text);
      }
      list.push({ kind: 'access', parts, span: join(start, this.tokens[this.idx - 1]!.span) });
      if (this.atPunct(',')) { this.next(); continue; }
      break;
    }
    return list;
  }

  private parseType(): TypeExpr {
    const t = this.peek();
    if (t.kind === 'kw' && RESERVED_KW_CODE.has(t.text)) {
      const code = RESERVED_KW_CODE.get(t.text)!;
      this.error(code, t.span, `type '${t.text}' is refused in L1 (${code})`);
      this.next();
      throw ParseError;
    }
    if (t.kind !== 'ident' && !(t.kind === 'kw' && SOFT_KEYWORDS.has(t.text))) {
      this.error('FORM-E-100', t.span, `expected a type name, found '${t.text}'`);
      throw ParseError;
    }
    const nameTok = this.next();
    if (this.atPunct('<')) {
      this.error('FORM-E-702', this.peek().span,
        "generics beyond pool capacity literals are refused in L1 (FORM-E-702; '<T>' syntax)");
      this.next();
      throw ParseError;
    }
    if (this.atPunct('[')) {
      this.next();
      let len: { kind: 'int'; value: bigint } | { kind: 'name'; name: string };
      if (this.peek().kind === 'int') {
        len = { kind: 'int', value: this.expectInt('array length').value };
      } else if (this.atIdent()) {
        len = { kind: 'name', name: this.expectName('array length const').text };
      } else {
        this.error('FORM-E-719', this.peek().span,
          `array length must be a constant, found '${this.peek().text}' (dynamic-length collections are refused, FORM §6)`);
        this.syncStmt();
        throw ParseError;
      }
      this.expectPunct(']');
      return { kind: 'array', elem: { kind: 'named', name: nameTok.text, span: nameTok.span }, len, span: nameTok.span };
    }
    return { kind: 'named', name: nameTok.text, span: nameTok.span };
  }

  // -- field declarations (@earth/@flow, §6) --------------------------------

  private parseFieldDecl(): TopDecl {
    const at = this.expectPunct('@');
    const profTok = this.peek();
    let profile: string;
    if (profTok.kind === 'ident' || (profTok.kind === 'kw' && RESERVED_KW_CODE.has(profTok.text))) {
      // domain keywords are keywords so @warp-style profiles fail as refusals
      profile = this.next().text;
    } else {
      this.error('FORM-E-100', profTok.span, `expected a profile name after '@', found '${profTok.text}'`);
      profile = '<bad>';
      this.next();
    }
    if (profile !== 'earth' && profile !== 'flow') {
      const code = (profile === 'build' || profile === 'warp' || profile === 'formation' || profile === 'stamp')
        ? 'FORM-E-715' : 'FORM-E-650';
      this.error(code, profTok.span,
        code === 'FORM-E-715'
          ? `@${profile} is an L2 profile, refused in L1 (@earth/@flow only, FORM-E-715)`
          : `@${profile} is not an admitted field profile (earth/flow only, FORM-E-650)`);
    }
    this.expectKw('field');
    const nameTok = this.expectName('field program name');
    const name = nameTok.text;
    this.expectPunct('(');
    let paramsStruct: string | null = null;
    let paramsSpan: SourceSpan | null = null;
    if (this.atKw('params')) {
      paramsSpan = this.next().span;
      this.expectPunct(':');
      paramsStruct = this.expectName('params struct type').text;
    }
    this.expectPunct(')');
    this.expectPunct('->');
    const retTok = this.peek();
    const returns = this.expectName('field return record', true).text;
    void retTok;
    const footprint = this.parseFootprint();
    this.expectKw('max_ops');
    const maxOps = this.expectInt('max_ops').value;
    this.expectPunct('{');
    const body: FieldStmt[] = [];
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const t = this.peek();
      if (t.kind === 'kw' && t.text === 'let') {
        try {
          this.next();
          const v = this.expectName('field local name');
          this.expectPunct('=');
          const value = this.parseExpr(DEFAULT_OPTS);
          this.expectPunct(';');
          body.push({ kind: 'field_let', name: v.text, value, span: join(t.span, this.tokens[this.idx - 1]!.span) });
        } catch { this.syncStmt(); }
        continue;
      }
      if (t.kind === 'kw' && t.text === 'return') {
        try {
          const rspan = this.next().span;
          const record = this.expectName('return record type', true).text;
          const fields = this.parseRecordFields();
          this.expectPunct(';');
          body.push({ kind: 'field_return', record, fields, span: join(rspan, this.tokens[this.idx - 1]!.span) });
        } catch { this.syncStmt(); }
        continue;
      }
      if (t.kind === 'kw' && (t.text === 'if' || t.text === 'for')) {
        this.error('FORM-E-651', t.span,
          `'${t.text}' in a field body violates the branchless law (the if select-EXPRESSION is the only conditional, FORM-E-651)`);
        this.skipStmtBody();
        continue;
      }
      if (t.kind === 'kw' && t.text === 'return') continue;
      this.error('FORM-E-653', t.span,
        `statement '${t.text}' is not admitted in a field body (only let/return, FORM-E-653)`);
      this.skipStmtBody();
    }
    this.expectPunct('}');
    return {
      kind: 'Field', profile, profileSpan: profTok.span, name, paramsStruct, paramsSpan,
      returns, footprint, maxOps, body, span: join(at.span, this.tokens[this.idx - 1]!.span),
    };
  }

  private skipStmtBody(): void {
    let depth = 0;
    while (this.peek().kind !== 'eof') {
      const t = this.peek();
      if (depth === 0 && t.kind === 'punct' && t.text === ';') { this.next(); return; }
      if (depth === 0 && t.kind === 'punct' && t.text === '}') return;
      this.next();
      if (t.kind === 'punct' && t.text === '{') depth++;
      if (t.kind === 'punct' && t.text === '}') depth--;
    }
  }

  private parseFootprint(): Footprint {
    this.expectKw('footprint');
    const t = this.peek();
    if (t.kind === 'kw' && t.text === 'none') { this.next(); this.expectPunct(';'); return { kind: 'none', span: t.span }; }
    // capsule arity 5 per §6.4 prose (ax, az, bx, bz, r) — the EBNF's 4-arg
    // form cannot express the swept-disc radius (spec-issue note, W3.2)
    const forms: Record<string, number> = { rect: 4, circle: 3, capsule: 5 };
    if (t.kind === 'kw' && forms[t.text] !== undefined) {
      this.next();
      this.expectPunct('(');
      const args: Expr[] = [];
      for (let k = 0; k < forms[t.text]!; k++) {
        args.push(this.parseExpr());
        if (k < forms[t.text]! - 1) this.expectPunct(',');
      }
      this.expectPunct(')');
      this.expectPunct(';');
      return { kind: t.text as 'rect' | 'circle' | 'capsule', args, span: t.span };
    }
    this.error('FORM-E-100', t.span,
      `expected a footprint form (none/rect/circle/capsule), found '${t.text}'`);
    throw ParseError;
  }

  // -- presentation / scenario / sound --------------------------------------

  private parsePresentation(): PresentationDecl {
    const kw = this.expectKw('presentation');
    const name = this.expectName('presentation name');
    this.expectPunct('{');
    const items: PresentationItem[] = [];
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const t = this.peek();
      try {
        if (t.kind === 'kw' && t.text === 'view') { items.push(this.parseView()); continue; }
        if (t.kind === 'kw' && t.text === 'shared') {
          this.next();
          this.expectKw('budget');
          const pct = this.parsePercent('shared budget');
          this.expectPunct(';');
          items.push({ kind: 'shared_budget', pct, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'emit') { items.push(this.parseEmit()); continue; }
        // statements are not admitted in presentation bodies (E-405/E-108
        // at the checker; assignment-shaped input still parses for the span)
        const stmt = this.parseStmtInBody('present');
        if (stmt) items.push(stmt);
      } catch { this.syncStmt(); }
    }
    this.expectPunct('}');
    return { kind: 'Presentation', name: name.text, items, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  /** `45%` — the lexer's percent token or a spaced `45 %` pair. */
  private parsePercent(what: string): bigint {
    const t = this.peek();
    if (t.kind === 'frac' && t.frac?.suffix === '%') {
      this.next();
      return BigInt(t.frac.intDigits);
    }
    if (t.kind === 'int') {
      this.next();
      if (this.atPunct('%')) { this.next(); return t.intVal!; }
      this.error('FORM-E-100', t.span, `expected a percent literal for ${what} (e.g. 45%)`);
      throw ParseError;
    }
    this.error('FORM-E-100', t.span, `expected a percent literal for ${what}, found '${t.text}'`);
    throw ParseError;
  }

  private parseView(): ViewItem {
    const kw = this.expectKw('view');
    const id = this.expectInt('view id').value;
    let camera: Expr | null = null;
    if (this.atKw('from')) {
      this.next();
      camera = this.parseExpr({ allowRange: false, recordLits: true });
    }
    this.expectKw('budget');
    const budgetPct = this.parsePercent('view budget');
    this.expectPunct(';');
    return { kind: 'view', id, camera, budgetPct, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseEmit(): EmitStmt {
    const kw = this.expectKw('emit');
    const t = this.peek();
    const KNOWN = new Set(['draw_form', 'draw_population', 'draw_procedural', 'surface_stamp', 'audio']);
    const ORDERING = new Set(['layer', 'after', 'transparent_group', 'barrier']);
    let emitKind: string;
    if (t.kind === 'kw' && KNOWN.has(t.text)) {
      emitKind = this.next().text;
    } else if (t.kind === 'ident' && ORDERING.has(t.text)) {
      this.error('FORM-E-717', t.span,
        `'${t.text}' ordering constructs are refused in L1 (L3; source order is not a synchronization primitive, FORM §13)`);
      emitKind = this.next().text;
    } else if (t.kind === 'ident' || t.kind === 'kw') {
      this.error('FORM-E-603', t.span, `unknown emit kind '${t.text}' (L1: draw_form/draw_population/draw_procedural/surface_stamp/audio)`);
      emitKind = this.next().text;
    } else {
      this.error('FORM-E-603', t.span, `expected an emit kind, found '${t.text}'`);
      emitKind = '<bad>';
      this.next();
    }
    const args: EmitArg[] = [];
    if (this.atPunct('(')) {
      this.next();
      while (!this.atPunct(')') && this.peek().kind !== 'eof') {
        const name = this.expectName('emit argument name', true);
        this.expectPunct(':');
        args.push({ kind: 'emit_arg', name: name.text, value: this.parseExpr(), span: name.span });
        if (this.atPunct(',')) { this.next(); continue; }
        break;
      }
      this.expectPunct(')');
    }
    this.expectPunct(';');
    return { kind: 'emit', emitKind, args, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseScenario(): ScenarioDecl {
    const kw = this.expectKw('scenario');
    const name = this.expectName('scenario name');
    this.expectPunct('{');
    const items: ScenarioItem[] = [];
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const t = this.peek();
      try {
        if (t.kind === 'kw' && t.text === 'seed') {
          this.next();
          const v = this.expectInt('seed').value;
          this.expectPunct(';');
          items.push({ kind: 'seed', value: v, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'load') {
          this.next();
          const target = this.expectName('load target');
          this.expectPunct(';');
          items.push({ kind: 'load', target: target.text, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'spawn') {
          this.next();
          this.expectKw('player');
          const index = this.expectInt('player index').value;
          this.expectKw('at');
          const at = this.expectName('spawn position global');
          this.expectPunct(';');
          items.push({ kind: 'spawn_player', index, at: at.text, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'at') {
          this.next();
          const tick = this.expectInt('tick').value;
          if (!this.atKw('tick') && !this.atKw('ticks')) {
            this.error('FORM-E-100', this.peek().span, `expected 'tick'/'ticks', found '${this.peek().text}'`);
            throw ParseError;
          }
          this.next();
          const action = this.parseExpr();
          this.expectPunct(';');
          items.push({ kind: 'at', tick, action, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'assert') {
          this.next();
          const expr = this.parseExpr();
          let tolerance: Expr | null = null;
          if (this.atIdent('within')) {
            this.next();
            tolerance = this.parseExpr();
          }
          this.expectPunct(';');
          items.push({ kind: 'assert', expr, tolerance, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'capture') {
          this.next();
          if (!this.atIdent('frame')) {
            this.error('FORM-E-100', this.peek().span, `expected 'frame', found '${this.peek().text}'`);
            throw ParseError;
          }
          this.next();
          const frame = this.expectInt('capture frame').value;
          this.expectKw('as');
          const nameTok = this.expectString('capture name');
          this.expectPunct(';');
          items.push({ kind: 'capture', frame, name: nameTok.value, span: t.span });
          continue;
        }
        if (t.kind === 'kw' && t.text === 'assert_budget') {
          // not a §1.1 keyword; usually parses as ident — handled below
        }
        if (t.kind === 'ident' && t.text === 'assert_budget') {
          this.next();
          const set = this.expectName('budget set name');
          this.expectPunct(';');
          items.push({ kind: 'assert_budget', budgetSet: set.text, span: t.span });
          continue;
        }
        // anything else: a statement in a scenario body
        this.parseStmtInBody('scenario');
      } catch { this.syncStmt(); }
    }
    this.expectPunct('}');
    return { kind: 'Scenario', name: name.text, items, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseSound(): SoundDecl {
    const kw = this.expectKw('sound');
    const name = this.expectName('sound name');
    this.expectPunct('{');
    this.expectKw('sample');
    const sample = this.expectString('sample path');
    this.expectPunct(';');
    const params: { kind: 'gain' | 'pitch' | 'pan'; value: Expr; span: SourceSpan }[] = [];
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const t = this.peek();
      if ((t.kind === 'kw' || t.kind === 'ident') && (t.text === 'gain' || t.text === 'pitch' || t.text === 'pan')) {
        this.next();
        const value = this.parseExpr();
        this.expectPunct(';');
        params.push({ kind: t.text, value, span: t.span });
        continue;
      }
      // audio-graph constructs beyond the tone event (L4)
      this.error('FORM-E-718', t.span,
        `tone parameter '${t.text}' is an audio-graph construct beyond the L1 tone-event declaration (L4, FORM-E-718)`);
      this.skipStmtBody();
    }
    this.expectPunct('}');
    return { kind: 'Sound', name: name.text, sample: sample.value, sampleSpan: sample.span, params, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  // -- statements -----------------------------------------------------------

  /** Parse statements until '}' (not consumed). Contexts: system/fn/present/scenario. */
  private parseStmts(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt[] {
    const stmts: Stmt[] = [];
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const before = this.idx;
      const s = this.parseStmtInBody(context);
      if (s) stmts.push(s);
      if (this.idx === before) this.next(); // guarantee progress
    }
    return stmts;
  }

  /** Returns BadStmt placeholders for statements parsed-but-refused. */
  private parseStmtInBody(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt | null {
    const t = this.peek();
    try {
      if (t.kind === 'kw') {
        switch (t.text) {
          case 'let': return this.parseLet();
          case 'if': return this.parseIfStmt(context);
          case 'for': return this.parseFor(context);
          case 'return': return this.parseReturn(context);
          case 'spawn': {
            if (this.atPunct('(', 1)) return this.parseSpawnCall();
            this.error('FORM-E-409', t.span, `'spawn player' is a scenario statement (FORM-E-409 outside a scenario)`);
            this.skipStmtBody();
            return { kind: 'bad_stmt', text: t.text, span: t.span };
          }
          case 'emit': {
            if (context !== 'present') {
              this.error('FORM-E-600', t.span, 'emit statements are only admitted inside a presentation block');
            }
            this.parseEmit();
            return { kind: 'bad_stmt', text: 'emit', span: t.span };
          }
          case 'seed': case 'load': case 'at': case 'capture': case 'assert': {
            if (context === 'scenario') { this.syncAfterBad(); return null; }
            this.error('FORM-E-409', t.span, `'${t.text}' is a scenario statement (FORM-E-409 outside a scenario block)`);
            this.skipStmtBody();
            return { kind: 'bad_stmt', text: t.text, span: t.span };
          }
          default: {
            if (RESERVED_KW_CODE.has(t.text)) {
              const code = RESERVED_KW_CODE.get(t.text)!;
              this.error(code, t.span, `'${t.text}' is refused in L1 (${code})`);
              this.skipStmtBody();
              return { kind: 'bad_stmt', text: t.text, span: t.span };
            }
            this.error('FORM-E-108', t.span,
              `statement '${t.text}' is not admitted in this context (FORM-E-108)`);
            this.skipStmtBody();
            return { kind: 'bad_stmt', text: t.text, span: t.span };
          }
        }
      }

      if (t.kind === 'ident') {
        // kill(pool, i);
        if (t.text === 'kill' && this.atPunct('(', 1)) return this.parseKill();
        // apply terrain_field name(...) duration Nt;
        if (t.text === 'apply' && (this.atIdent('terrain_field', 1) || this.atIdent('flow', 1))) {
          return this.parseApply(context);
        }
        // assert_budget x; (scenario-only)
        if (t.text === 'assert_budget' && this.atIdent(undefined, 1) && context !== 'scenario') {
          this.error('FORM-E-409', t.span, "'assert_budget' is a scenario statement (FORM-E-409 outside a scenario block)");
          this.skipStmtBody();
          return { kind: 'bad_stmt', text: t.text, span: t.span };
        }
      }

      // general expression / assignment statement
      const expr = this.parseExpr();
      if (this.atPunct('=')) {
        this.next();
        const value = this.parseExpr();
        this.expectPunct(';');
        return { kind: 'assign', target: expr, value, span: join(expr.span, this.tokens[this.idx - 1]!.span) };
      }
      this.expectPunct(';');
      if (expr.kind !== 'call') {
        this.error('FORM-E-110', expr.span,
          'expression statements must be calls (an expression with no effect is not a statement)');
        return { kind: 'bad_stmt', text: '', span: expr.span };
      }
      return { kind: 'call_stmt', call: expr, span: expr.span };
    } catch {
      this.syncStmt();
      return null;
    }
  }

  private syncAfterBad(): void { this.syncStmt(); }

  private parseLet(): Stmt {
    const kw = this.expectKw('let');
    const name = this.expectName('let name');
    let type: TypeExpr | null = null;
    if (this.atPunct(':')) { this.next(); type = this.parseType(); }
    this.expectPunct('=');
    const init = this.parseExpr();
    this.expectPunct(';');
    return { kind: 'let', name: name.text, type, init, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseIfStmt(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt {
    const kw = this.expectKw('if');
    const cond = this.parseExpr({ allowRange: false, recordLits: false });
    this.expectPunct('{');
    const then = this.parseStmts(context);
    this.expectPunct('}');
    let elseBranch: Stmt[] | Stmt | null = null;
    if (this.atKw('else')) {
      this.next();
      if (this.atKw('if')) elseBranch = this.parseIfStmt(context);
      else {
        this.expectPunct('{');
        elseBranch = this.parseStmts(context);
        this.expectPunct('}');
      }
    }
    return { kind: 'if', cond, then, else: elseBranch, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseFor(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt {
    const kw = this.expectKw('for');
    const varName = this.expectName('loop variable').text;
    this.expectKw('in');
    let range: { kind: 'range'; lo: Expr; hi: Expr } | { kind: 'pool'; pool: string; poolSpan: SourceSpan };
    if (this.atIdent() && this.atPunct('{', 1)) {
      const poolTok = this.next();
      range = { kind: 'pool', pool: poolTok.text, poolSpan: poolTok.span };
    } else {
      const lo = this.parseExpr({ allowRange: true, recordLits: false });
      if (lo.kind !== 'range') {
        this.error('FORM-E-100', this.peek().span,
          `expected '..' in a for range (or a pool name for pool sugar), found '${this.peek().text}'`);
        throw ParseError;
      }
      range = { kind: 'range', lo: lo.lo, hi: lo.hi };
    }
    this.expectPunct('{');
    const body = this.parseStmts(context);
    this.expectPunct('}');
    return { kind: 'for', varName, range, body, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseReturn(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt {
    const kw = this.expectKw('return');
    let value: Expr | null = null;
    if (!this.atPunct(';')) value = this.parseExpr();
    this.expectPunct(';');
    const s: Stmt = { kind: 'return', value, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
    if (context !== 'fn') {
      this.error('FORM-E-108', kw.span,
        `'return' is only admitted in fn bodies (FORM-E-108 in a ${context} body)`);
    }
    return s;
  }

  private parseSpawnCall(): Stmt {
    const kw = this.expectKw('spawn');
    this.expectPunct('(');
    const pool = this.expectName('pool name');
    this.expectPunct(',');
    const value = this.parseRecordLitRequired('spawn value');
    this.expectPunct(')');
    this.expectPunct(';');
    return { kind: 'spawn', pool: pool.text, value, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseKill(): Stmt {
    const kw = this.peek();
    this.next(); // 'kill'
    this.expectPunct('(');
    const pool = this.expectName('pool name');
    this.expectPunct(',');
    const index = this.parseExpr();
    this.expectPunct(')');
    this.expectPunct(';');
    return { kind: 'kill', pool: pool.text, index, span: join(kw.span, this.tokens[this.idx - 1]!.span) };
  }

  private parseApply(context: 'system' | 'fn' | 'present' | 'scenario'): Stmt {
    const kw = this.next(); // 'apply'
    const kindTok = this.next(); // 'terrain_field' | 'flow'
    const program = this.expectName('field program name');
    const args: EmitArg[] = [];
    this.expectPunct('(');
    while (!this.atPunct(')') && this.peek().kind !== 'eof') {
      const name = this.expectName('apply argument name', true);
      this.expectPunct(':');
      args.push({ kind: 'emit_arg', name: name.text, value: this.parseExpr(), span: name.span });
      if (this.atPunct(',')) { this.next(); continue; }
      break;
    }
    this.expectPunct(')');
    const durTok = this.peek();
    if (!(durTok.kind === 'ident' && durTok.text === 'duration')) {
      this.error('FORM-E-463', durTok.span,
        `expected 'duration <ticks>' in an apply statement, found '${durTok.text}' (FORM-E-463)`);
      throw ParseError;
    }
    this.next();
    const duration = this.parseExpr();
    this.expectPunct(';');
    const s: ApplyStmt = {
      kind: 'apply', applyKind: kindTok.text, program: program.text, programSpan: program.span,
      args, duration, span: join(kw.span, this.tokens[this.idx - 1]!.span),
    };
    if (context !== 'system') {
      this.error('FORM-E-461', kw.span,
        `'apply' is a sim statement — terrain is truth (FORM-E-461 outside a system)`);
    }
    return s;
  }

  private parseRecordLitRequired(what: string): RecordLit {
    const t = this.peek();
    if (t.kind === 'ident') {
      const e = this.parseExpr();
      if (e.kind === 'record') return e;
      this.error('FORM-E-100', e.span, `expected a record literal for ${what}`);
      return { kind: 'record', typeName: '<bad>', fields: [], span: e.span };
    }
    this.error('FORM-E-100', t.span, `expected a record literal for ${what}, found '${t.text}'`);
    throw ParseError;
  }

  // -- Pratt expressions -----------------------------------------------------

  private static readonly BINOP_BP: ReadonlyMap<string, number> = new Map([
    ['||', 1], ['&&', 2], ['|', 3], ['^', 4], ['&', 5],
    ['<<', 6], ['>>', 6], ['==', 7], ['!=', 7],
    ['<', 8], ['<=', 8], ['>', 8], ['>=', 8],
    ['..', 9],
    ['+', 10], ['-', 10], ['*', 11], ['/', 11],
  ]);

  parseExpr(opts: ExprOpts = DEFAULT_OPTS): Expr {
    return this.parseExprBp(0, opts);
  }

  private parseExprBp(minBp: number, opts: ExprOpts): Expr {
    let left = this.parseUnary(opts);

    while (true) {
      const t = this.peek();
      if (t.kind !== 'punct') break;
      if (t.text === '..' && !opts.allowRange) {
        this.error('FORM-E-107', t.span, "'..' is only admitted in a for header (FORM-E-107)");
        this.next();
        continue;
      }
      const bp = Parser.BINOP_BP.get(t.text);
      if (bp === undefined || bp < minBp) break;
      const opTok = this.next();


      // comparisons are non-associative (§2.2): `a < b < c` is refused
      if (['<', '<=', '>', '>='].includes(opTok.text) && left.kind === 'binary'
          && ['<', '<=', '>', '>='].includes((left as unknown as { op: string }).op)) {
        this.error('FORM-E-110', opTok.span,
          'comparisons are non-associative (parenthesize the inner comparison)');
        throw ParseError;
      }

      if (opTok.text === '..') {
        const hi = this.parseExprBp(bp + 1, { ...opts, allowRange: false });
        left = { kind: 'range', lo: left, hi, span: join(left.span, hi.span) };
        continue;
      }
      const right = this.parseExprBp(bp + 1, opts);
      // closure syntax detection: `(a, b) -> ...` / `x -> ...`
      if (this.atPunct('->')) {
        this.error('FORM-E-704', this.peek().span,
          "'->' in expression position is closure/lambda syntax, refused in L1 (FORM-E-704)");
        this.next();
      }
      left = { kind: 'binary', op: opTok.text, l: left, r: right, span: join(left.span, right.span) };
    }
    return left;
  }

  private parseUnary(opts: ExprOpts): Expr {
    const t = this.peek();
    if (t.kind === 'punct' && (t.text === '-' || t.text === '!' || t.text === '~')) {
      this.next();
      const operand = this.parseUnary(opts);
      return { kind: 'unary', op: t.text as '-' | '!' | '~', operand, span: join(t.span, operand.span) };
    }
    if (t.kind === 'punct' && (t.text === '*' || t.text === '&')) {
      this.error('FORM-E-707', t.span,
        `prefix '${t.text}' is pointer/reference syntax, refused in L1 (FORM-E-707)`);
      this.next();
      const operand = this.parseUnary(opts);
      return { kind: 'unary', op: '-', operand, span: join(t.span, operand.span) };
    }
    if (t.kind === 'kw' && t.text === 'fn') {
      this.error('FORM-E-704', t.span, 'function expressions are closure syntax, refused in L1 (FORM-E-704)');
      this.next();
      return { kind: 'ident', name: '<bad>', span: t.span };
    }
    return this.parsePostfix(opts);
  }

  private parsePostfix(opts: ExprOpts): Expr {
    let expr = this.parsePrimary(opts);
    while (true) {
      const t = this.peek();
      if (t.kind === 'punct' && t.text === '.') {
        this.next();
        const f = this.expectName('field name', true);
        expr = { kind: 'member', obj: expr, field: f.text, fieldSpan: f.span, span: join(expr.span, f.span) };
        continue;
      }
      if (t.kind === 'punct' && t.text === '(') {
        this.next();
        const args: Expr[] = [];
        while (!this.atPunct(')') && this.peek().kind !== 'eof') {
          args.push(this.parseExpr(opts));
          if (this.atPunct(',')) { this.next(); continue; }
          break;
        }
        const close = this.expectPunct(')');
        expr = { kind: 'call', callee: expr, args, span: join(expr.span, close.span) };
        continue;
      }
      if (t.kind === 'punct' && t.text === '[') {
        this.next();
        const index = this.parseExpr(opts);
        const close = this.expectPunct(']');
        expr = { kind: 'index', obj: expr, index, span: join(expr.span, close.span) };
        continue;
      }
      break;
    }
    if (this.atPunct('->')) {
      this.error('FORM-E-704', this.peek().span,
        "'->' after an expression is closure/lambda syntax, refused in L1 (FORM-E-704)");
      this.next();
    }
    return expr;
  }

  private parsePrimary(opts: ExprOpts): Expr {
    const t = this.peek();

    switch (t.kind) {
      case 'int': case 'tick': case 'colour': {
        this.next();
        return {
          kind: 'literal',
          lit: t.kind === 'int' ? 'int' : t.kind === 'tick' ? 'tick' : 'colour',
          text: t.text, intVal: t.intVal, span: t.span,
        };
      }
      case 'frac': {
        this.next();
        return { kind: 'literal', lit: 'frac', text: t.text, frac: t.frac, span: t.span };
      }
      case 'string': {
        this.next();
        return { kind: 'string', value: t.text, span: t.span };
      }
      case 'kw': {
        if (t.text === 'true' || t.text === 'false') {
          this.next();
          return { kind: 'literal', lit: 'bool', text: t.text, span: t.span };
        }
        if (t.text === 'if') return this.parseIfExpr();
        // keyword-led paths: input.player / random.stream / sample.x / p.x / params.f
        if (SOFT_KEYWORDS.has(t.text) || t.text === 'random') {
          this.next();
          if (this.atPunct('.')) {
            return { kind: 'ident', name: t.text, span: t.span };
          }
          this.error('FORM-E-109', t.span, `keyword '${t.text}' cannot be used as a value`);
          return { kind: 'ident', name: '<bad>', span: t.span };
        }
        if (RESERVED_KW_CODE.has(t.text)) {
          const code = RESERVED_KW_CODE.get(t.text)!;
          this.error(code, t.span, `'${t.text}' is refused in L1 (${code})`);
          this.next();
          return { kind: 'ident', name: '<bad>', span: t.span };
        }
        this.error('FORM-E-110', t.span, `'${t.text}' cannot begin an expression`);
        this.next();
        return { kind: 'ident', name: '<bad>', span: t.span };
      }
      case 'ident': {
        // record literal: ident '{' [ident '='] ... (2-token lookahead —
        // the record-literal/block ambiguity is resolved in favour of a
        // record literal only when a field assignment follows the '{')
        if (opts.recordLits && this.atPunct('{', 1)) {
          const after = this.peek(2);
          const isField = (after.kind === 'ident' || (after.kind === 'kw' && SOFT_KEYWORDS.has(after.text)))
            && this.atPunct('=', 3);
          if (isField) return this.parseRecordLit();
        }
        this.next();
        return { kind: 'ident', name: t.text, span: t.span };
      }
      case 'punct': {
        if (t.text === '(') {
          this.next();
          const inner = this.parseExpr(opts);
          this.expectPunct(')');
          if (this.atPunct('->')) {
            this.error('FORM-E-704', this.peek().span,
              "'->' after a parenthesized expression is lambda syntax, refused in L1 (FORM-E-704)");
            this.next();
          }
          return inner;
        }
        this.error('FORM-E-110', t.span, `'${t.text}' cannot begin an expression`);
        this.next();
        return { kind: 'ident', name: '<bad>', span: t.span };
      }
      default: {
        this.error('FORM-E-110', t.span, `unexpected '${t.text}' in expression`);
        this.next();
        return { kind: 'ident', name: '<bad>', span: t.span };
      }
    }
  }

  /** `if cond { expr } else { expr }` / `else if` — the select-expression. */
  private parseIfExpr(): Expr {
    const kw = this.next(); // 'if'
    const cond = this.parseExpr({ allowRange: false, recordLits: false });
    this.expectPunct('{');
    const thenExpr = this.parseExpr({ allowRange: false, recordLits: true });
    this.expectPunct('}');
    this.expectKw('else');
    let elseExpr: Expr;
    if (this.atKw('if')) {
      elseExpr = this.parseIfExpr();
    } else {
      this.expectPunct('{');
      elseExpr = this.parseExpr({ allowRange: false, recordLits: true });
      this.expectPunct('}');
    }
    return { kind: 'if_expr', cond, then: thenExpr, else: elseExpr, span: join(kw.span, elseExpr.span) };
  }

  private parseRecordLit(): RecordLit {
    const nameTok = this.next(); // ident
    const fields = this.parseRecordFields();
    return { kind: 'record', typeName: nameTok.text, fields, span: join(nameTok.span, this.tokens[this.idx - 1]!.span) };
  }

  /** Parses `{ f = e, ... }` including both braces. */
  private parseRecordFields(): RecordField[] {
    this.expectPunct('{');
    const fields: RecordField[] = [];
    const seen = new Set<string>();
    while (!this.atPunct('}') && this.peek().kind !== 'eof') {
      const f = this.expectName('record field name', true);
      this.expectPunct('=');
      const value = this.parseExpr();
      if (seen.has(f.text)) {
        this.error('FORM-E-106', f.span, `duplicate field '${f.text}' in record literal`);
      }
      seen.add(f.text);
      fields.push({ name: f.text, value, span: f.span });
      if (this.atPunct(',')) { this.next(); continue; }
      break;
    }
    this.expectPunct('}');
    return fields;
  }
}

/** Sentinel thrown by expect* helpers; caught at statement/decl granularity. */
const ParseError = new Error('parse-error-sentinel');

function join(a: SourceSpan, b: SourceSpan): SourceSpan {
  return { file: a.file, start: Math.min(a.start, b.start), end: Math.max(a.end, b.end) };
}

/** Lex + parse one module (the usual entry point). */
export function parseSource(source: string | Uint8Array, file: string, sink: DiagnosticSink): ModuleAst {
  const bytes = typeof source === 'string' ? new TextEncoder().encode(source) : source;
  const tokens = tokenize(bytes, file, sink);
  return new Parser(tokens, sink, file).parseModule();
}

export { span };
