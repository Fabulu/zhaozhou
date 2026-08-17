// checker.ts — Form L1 type/effect/schedule checker (W3.2).
//
// Law: language-semantics.md §3 (types), §4 (declarations/scoping),
// domains-and-effects.md §2 (domain effect contracts, present purity),
// deterministic-scheduling.md §3 (one writer per component per phase, D6),
// language-semantics.md §6 (field dialect admission onto the frozen
// FieldBuilder). Diagnostics are collected, never thrown.

import {
  ConstDecl, EmitStmt, EnumDecl, Expr, FieldDecl, FieldStmt, FnDecl, GlobalDecl,
  ModuleAst, PoolDecl, PresentationDecl, RecordLit, ScenarioDecl, SoundDecl,
  Stmt, StructDecl, SystemDecl, TypeExpr,
} from './ast.js';
import { DiagnosticSink } from './diagnostics.js';
import { SourceSpan } from './span.js';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

/** literal-law routing flag for the scenario tolerance position (E-907) */
let ctxInTolerance = false;

export type Type =
  | { t: 'fx16' } | { t: 'fx24' } | { t: 'angle16' } | { t: 'unit8' }
  | { t: 'i32' } | { t: 'u32' } | { t: 'bool' } | { t: 'world2' }
  | { t: 'world3' } | { t: 'velocity3' } | { t: 'colour8' } | { t: 'padframe' }
  | { t: 'stream' } | { t: 'void' } | { t: 'unknown' } | { t: 'sound' }
  | { t: 'enum'; name: string }
  | { t: 'struct'; name: string }
  | { t: 'array'; elem: Type; len: number }
  | { t: 'pool'; name: string; struct: string };

export const T = {
  fx16: { t: 'fx16' } as Type, fx24: { t: 'fx24' } as Type,
  angle16: { t: 'angle16' } as Type, unit8: { t: 'unit8' } as Type,
  i32: { t: 'i32' } as Type, u32: { t: 'u32' } as Type, bool: { t: 'bool' } as Type,
  world2: { t: 'world2' } as Type, world3: { t: 'world3' } as Type,
  velocity3: { t: 'velocity3' } as Type, colour8: { t: 'colour8' } as Type,
  padframe: { t: 'padframe' } as Type, stream: { t: 'stream' } as Type,
  void: { t: 'void' } as Type, unknown: { t: 'unknown' } as Type,
  sound: { t: 'sound' } as Type,
};

export function typeName(ty: Type): string {
  if (tAgree(ty, T.void)) return 'void';
  if (tAgree(ty, T.unknown)) return '<unknown>';
  if (ty.t === 'enum') return ty.name;
  if (ty.t === 'struct') return ty.name;
  if (ty.t === 'array') return `${typeName(ty.elem)}[${ty.len}]`;
  if (ty.t === 'pool') return `pool ${ty.name}`;
  return ty.t;
}

export function tAgree(a: Type, b: Type): boolean {
  if (a.t !== b.t) return false;
  if (a.t === 'enum' || a.t === 'struct') return a.name === (b as { name: string }).name;
  if (a.t === 'pool') return a.name === (b as { name: string }).name;
  if (a.t === 'array') return a.len === (b as { len: number }).len && tAgree(a.elem, (b as { elem: Type }).elem);
  return true;
}

const SCALARS: Record<string, Type> = {
  fx16: T.fx16, fx24: T.fx24, angle16: T.angle16, unit8: T.unit8,
  i32: T.i32, u32: T.u32, bool: T.bool, world2: T.world2, world3: T.world3,
  velocity3: T.velocity3, colour8: T.colour8,
};

const NUMERIC = new Set(['fx16', 'fx24', 'angle16', 'unit8', 'i32', 'u32']);
const VECTORS = new Set(['world2', 'world3', 'velocity3']);
const INTS = new Set(['i32', 'u32']);

// ---------------------------------------------------------------------------
// Symbols + modules
// ---------------------------------------------------------------------------

type SymKind = 'const' | 'enum' | 'struct' | 'pool' | 'global' | 'fn' | 'system'
  | 'field' | 'presentation' | 'scenario' | 'sound';

interface Sym {
  kind: SymKind;
  decl: Record<string, unknown> & { span: SourceSpan; name?: string };
  order: number;
}

interface ModuleSym {
  ast: ModuleAst;
  table: Map<string, Sym>;
}

type Domain = 'sim' | 'fn' | 'present' | 'scenario' | 'field';

interface LocalInfo {
  type: Type;
  letSpan: SourceSpan;
  assigned: boolean;
  isParam: boolean;
}

interface Ctx {
  mod: ModuleSym;
  domain: Domain;
  profile?: 'earth' | 'flow';
  fieldDecl?: FieldDecl;
  reads: Set<string>;
  writes: Set<string>;
  locals: Map<string, LocalInfo>;
  fnRet?: Type;
  fnHasReturn?: boolean;
  /** pool name when inside a pool-sugar loop (spawn/kill refused inside) */
  loopPool: string | null;
  inLoop: boolean;
  /** let names appearing anywhere in the enclosing body (use-before-let, E-303) */
  laterLets?: Set<string>;
  /** inside a scenario `assert` — unknown names are E-906, not E-203 */
  inAssert?: boolean;
  /** inside a scenario assert tolerance — non-exact fx16 is E-907, not E-008 */
  inTolerance?: boolean;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

export interface ScheduledSystem {
  name: string;
  module: string;
  every: bigint;
  span: SourceSpan;
}

export interface Schedule {
  /** phase 0 runs first (deterministic-scheduling.md §3) */
  phases: { index: number; systems: ScheduledSystem[] }[];
}

export interface CheckResult {
  schedule: Schedule | null;
  /** Exact type of every expression admitted by the checker, keyed by AST identity. */
  expressionTypes: ReadonlyMap<Expr, Type>;
}

export function checkModules(modules: ModuleAst[], sink: DiagnosticSink): CheckResult {
  const c = new Checker(modules, sink);
  return c.run();
}

// ---------------------------------------------------------------------------
// The checker
// ---------------------------------------------------------------------------

class Checker {
  private readonly mods: ModuleSym[] = [];
  private readonly byName = new Map<string, ModuleSym>();
  /** fn call graph: module::fn -> edges [{to, span}] */
  private readonly fnEdges = new Map<string, { to: string; span: SourceSpan }[]>();
  /** Successful type-check result for each source expression (AST object identity). */
  private readonly expressionTypes = new Map<Expr, Type>();

  constructor(private readonly modules: ModuleAst[], private readonly sink: DiagnosticSink) {}

  run(): CheckResult {
    // pass 1: symbol tables (E-201 duplicates, E-832 registry overflow)
    for (const m of this.modules) this.buildTable(m);

    // pass 2: import resolution (E-202/204/205/206)
    this.checkImports();

    // pass 3: type-level declarations (const/enum/struct/pool/global)
    for (const ms of this.mods) {
      for (const d of ms.ast.decls) {
        if (d.kind === 'Const') this.checkConst(ms, d);
        else if (d.kind === 'Enum') this.checkEnum(ms, d);
        else if (d.kind === 'Struct') this.checkStruct(ms, d);
        else if (d.kind === 'Pool') this.checkPool(ms, d);
        else if (d.kind === 'Global') this.checkGlobal(ms, d);
      }
    }
    this.checkStructRecursion();

    // pass 4: bodies
    for (const ms of this.mods) {
      for (const d of ms.ast.decls) {
        if (d.kind === 'Fn') this.checkFn(ms, d);
        else if (d.kind === 'System') this.checkSystem(ms, d);
        else if (d.kind === 'Field') this.checkField(ms, d);
        else if (d.kind === 'Presentation') this.checkPresentation(ms, d);
        else if (d.kind === 'Scenario') this.checkScenario(ms, d);
        else if (d.kind === 'Sound') this.checkSound(ms, d);
      }
    }

    // pass 5: recursion (E-709)
    this.checkRecursion();

    // pass 6: schedule (E-500/407/505) — the D6 analysis feeding W3.3
    const schedule = this.computeSchedule();
    return { schedule, expressionTypes: this.expressionTypes };
  }

  // -- pass 1: symbols -----------------------------------------------------

  private buildTable(m: ModuleAst): void {
    const ms: ModuleSym = { ast: m, table: new Map() };
    this.mods.push(ms);
    if (this.byName.has(m.name)) {
      this.sink.error('FORM-E-201', m.span, `module name '${m.name}' declared twice across the compilation`);
    }
    this.byName.set(m.name, ms);
    let order = 0;
    for (const d of m.decls) {
      const name = (d as unknown as { name?: string }).name;
      if (!name) continue;
      order++;
      if (ms.table.has(name)) {
        this.sink.error('FORM-E-201', d.span,
          `duplicate top-level declaration '${name}' in module '${m.name}'`);
        continue;
      }
      ms.table.set(name, { kind: d.kind.toLowerCase() as SymKind, decl: d as unknown as Sym['decl'], order });
    }
    // E-832: source-ID registry overflow (> 65536 declarations per module)
    if (m.decls.length > 65536) {
      this.sink.error('FORM-E-832', m.span,
        `module '${m.name}' declares ${m.decls.length} declarations — source-ID registry overflow (> 65536 per module)`);
    }
  }

  // -- pass 2: imports ------------------------------------------------------

  private checkImports(): void {
    // cycle detection over the module graph
    const state = new Map<string, 0 | 1 | 2>(); // 1 = on stack, 2 = done
    const cyclePath: string[] = [];
    const visit = (name: string): boolean => {
      const st = state.get(name) ?? 0;
      if (st === 2) return false;
      if (st === 1) return true; // cycle
      state.set(name, 1);
      cyclePath.push(name);
      const ms = this.byName.get(name)!;
      for (const imp of ms.ast.imports) {
        if (!this.byName.has(imp.module)) {
          this.sink.error('FORM-E-202', imp.span, `import of unknown module '${imp.module}'`);
          continue;
        }
        for (const n of imp.names) {
          const target = this.byName.get(imp.module)!.table.get(n);
          if (!target) {
            this.sink.error('FORM-E-203', imp.span,
              `module '${imp.module}' has no public name '${n}' to import`);
          } else if (n.startsWith('_')) {
            this.sink.error('FORM-E-206', imp.span,
              `'${n}' is private to module '${imp.module}' (underscore-prefixed names are module-private; use it qualified as ${imp.module}.${n}, FORM-E-206)`);
          }
        }
        if (visit(imp.module)) {
          this.sink.error('FORM-E-204', imp.span,
            `import cycle: ${[...cyclePath.slice(cyclePath.indexOf(imp.module)), imp.module, name].reverse().join(' -> ')}`);
        }
      }
      cyclePath.pop();
      state.set(name, 2);
      return false;
    };
    for (const ms of this.mods) visit(ms.ast.name);

    // ambiguous unqualified names (E-205): two selected imports bring one name
    for (const ms of this.mods) {
      const brought = new Map<string, string[]>();
      for (const imp of ms.ast.imports) {
        for (const n of imp.names) {
          const list = brought.get(n) ?? [];
          list.push(imp.module);
          brought.set(n, list);
        }
      }
      for (const [n, froms] of brought) {
        if (froms.length > 1 && !ms.table.has(n)) {
          this.sink.error('FORM-E-205', ms.ast.span,
            `ambiguous unqualified name '${n}' (brought by ${froms.map((f) => `'${f}'`).join(' and ')})`);
        }
      }
    }
  }

  /** Resolve an unqualified name visible in `ms` (ambiguous = two imports bring it). */
  private resolveUnqualified(ms: ModuleSym, name: string): { sym: Sym; mod: ModuleSym; ambiguous: boolean } | null {
    if (BUILTIN_CONSTS.has(name) && !ms.table.has(name) && !name.includes('.')) {
      return {
        sym: { kind: 'const', decl: { kind: 'Const', name, type: { kind: 'named', name: 'u32', span: ms.ast.span }, init: { kind: 'literal', lit: 'int', text: String(BUILTIN_CONSTS.get(name)), intVal: BUILTIN_CONSTS.get(name), span: ms.ast.span }, span: ms.ast.span } as unknown as Sym['decl'], order: -1 },
        mod: ms,
        ambiguous: false,
      };
    }
    const own = ms.table.get(name);
    if (own) return { sym: own, mod: ms, ambiguous: false };
    const found: { sym: Sym; mod: ModuleSym }[] = [];
    for (const imp of ms.ast.imports) {
      if (!imp.names.includes(name)) continue;
      const target = this.byName.get(imp.module);
      const sym = target?.table.get(name);
      if (sym) found.push({ sym, mod: target! });
    }
    if (found.length > 1) return { sym: found[0]!.sym, mod: found[0]!.mod, ambiguous: true };
    return found[0] ? { sym: found[0].sym, mod: found[0].mod, ambiguous: false } : null;
  }

  // -- pass 3: type-level declarations --------------------------------------

  private resolveType(ms: ModuleSym, te: TypeExpr, allowPool = false): Type {
    if (te.kind === 'array') {
      const elem = this.resolveType(ms, te.elem, allowPool);
      let len: number;
      if (te.len.kind === 'int') len = Number(te.len.value);
      else {
        const v = this.constEval(ms, { kind: 'ident', name: te.len.name, span: te.span } as Expr);
        if (v === null) { len = -1; }
        else len = Number(v);
      }
      if (len < 0) len = 0;
      return { t: 'array', elem, len };
    }
    if (SCALARS[te.name]) return SCALARS[te.name]!;
    const r = this.resolveUnqualified(ms, te.name);
    if (!r || r.ambiguous === true) {
      this.sink.error('FORM-E-301', te.span, `unknown type name '${te.name}'`);
      return T.unknown;
    }
    if (r.sym.kind === 'struct') return { t: 'struct', name: te.name };
    if (r.sym.kind === 'enum') return { t: 'enum', name: te.name };
    if (r.sym.kind === 'pool') {
      if (allowPool) return { t: 'pool', name: te.name, struct: (r.sym.decl as unknown as PoolDecl).structName };
      this.sink.error('FORM-E-301', te.span,
        `struct field type '${te.name}' is a pool — pools are not field types (language-semantics §4.2; spec-issue: §4.2 cites FORM-E-213 which the §7 catalog does not define, so the unknown-type code carries it)`);
      return T.unknown;
    }
    this.sink.error('FORM-E-301', te.span, `'${te.name}' is not a type`);
    return T.unknown;
  }

  private checkConst(ms: ModuleSym, d: ConstDecl): void {
    const ty = this.resolveType(ms, d.type);
    if (!this.isConstExpr(d.init)) {
      this.sink.error('FORM-E-210', d.init.span,
        'const initializer must be a constant expression (literals, consts, enum members, arithmetic)');
    }
    if (!/^_?[A-Z][A-Z0-9_]*$/.test(d.name)) {
      this.sink.warning('FORM-W-001', d.span,
        `const '${d.name}' is not SCREAMING_SNAKE_CASE (convention; warnings never affect semantics)`);
    }
    this.checkExpr(this.ctxFor(ms, 'fn'), d.init, ty);
  }

  private checkGlobal(ms: ModuleSym, d: GlobalDecl): void {
    const ty = this.resolveType(ms, d.type);
    if (!this.isConstExpr(d.init)) {
      this.sink.error('FORM-E-210', d.init.span,
        'global initializer must be a constant expression (explicit persistent state starts known)');
    }
    this.checkExpr(this.ctxFor(ms, 'fn'), d.init, ty);
  }

  private checkEnum(ms: ModuleSym, d: EnumDecl): void {
    let next = 0n;
    let prev: bigint | null = null;
    const seenNames = new Set<string>();
    const seenVals = new Map<bigint, string>();
    for (const m of d.members) {
      if (seenNames.has(m.name)) {
        this.sink.error('FORM-E-102', m.span, `duplicate enum member '${m.name}'`);
      }
      seenNames.add(m.name);
      const val = m.value ?? next;
      if (m.value === null) next = val + 1n;
      else next = val + 1n;
      if (prev !== null && val <= prev) {
        this.sink.error('FORM-E-211', m.span,
          `enum values must be ascending ('${m.name}' = ${val} follows ${prev})`);
      }
      const dup = seenVals.get(val);
      if (dup !== undefined) {
        this.sink.error('FORM-E-212', m.span,
          `enum value ${val} is not unique (already used by '${dup}')`);
      }
      seenVals.set(val, m.name);
      prev = val;
    }
    void ms;
  }

  private checkStruct(ms: ModuleSym, d: StructDecl): void {
    const seen = new Set<string>();
    for (const f of d.fields) {
      if (seen.has(f.name)) {
        this.sink.error('FORM-E-102', f.span, `duplicate struct field '${f.name}'`);
      }
      seen.add(f.name);
      this.resolveType(ms, f.type);
    }
  }

  private checkStructRecursion(): void {
    // DFS over struct field edges (bounded memory law, FORM §6)
    for (const ms of this.mods) {
      for (const d of ms.ast.decls) {
        if (d.kind !== 'Struct') continue;
        const stack: { name: string; span: SourceSpan }[] = [];
        const onStack = new Set<string>();
        const visit = (s: StructDecl): void => {
          if (onStack.has(s.name)) {
            const cycle = stack.slice(stack.findIndex((x) => x.name === s.name)).map((x) => x.name).join(' -> ');
            this.sink.error('FORM-E-801', s.span,
              `recursive struct type (${cycle || s.name}) — bounded memory law (FORM §6; carried by FORM-E-801's recursive clause, spec-issue note)`);
            return;
          }
          onStack.add(s.name);
          stack.push({ name: s.name, span: s.span });
          for (const f of s.fields) {
            const ft = f.type;
            if (ft.kind === 'named') {
              const r = this.resolveUnqualified(ms, ft.name);
              if (r && r.ambiguous !== true && r.sym.kind === 'struct') {
                visit(r.sym.decl as unknown as StructDecl);
              }
            }
          }
          stack.pop();
          onStack.delete(s.name);
        };
        visit(d);
      }
    }
  }

  /** Pool capacity must be a positive integer or a u32 const (E-800/E-801). */
  private checkPool(ms: ModuleSym, d: PoolDecl): void {
    let cap: bigint | null = null;
    if (d.capacity === null) {
      cap = 1n; // the parser already raised E-719/E-800 for the broken form
    } else if (d.capacity.kind === 'int') {
      cap = d.capacity.value;
    } else {
      const r = this.resolveUnqualified(ms, d.capacity.name);
      if (!r || r.ambiguous === true || r.sym.kind !== 'const') {
        this.sink.error('FORM-E-800', d.capacity.span,
          `pool capacity '${d.capacity.name}' is not a u32 const`);
      } else {
        const cd = r.sym.decl as unknown as ConstDecl;
        const cty = this.resolveType(r.mod, cd.type);
        if (!tAgree(cty, T.u32) && !tAgree(cty, T.i32)) {
          this.sink.error('FORM-E-800', d.capacity.span,
            `pool capacity const '${d.capacity.name}' must be u32`);
        }
        cap = this.constEval(r.mod, cd.init);
      }
    }
    if (cap !== null && cap <= 0n) {
      this.sink.error('FORM-E-800', d.span, `pool capacity must be positive (got ${cap})`);
    }
    const r = this.resolveUnqualified(ms, d.structName);
    if (!r || r.ambiguous === true || r.sym.kind !== 'struct') {
      this.sink.error('FORM-E-801', d.span,
        `pool element type '${d.structName}' is not a struct`);
    }
  }

  // -- pass 4a: functions ----------------------------------------------------

  private ctxFor(ms: ModuleSym, domain: Domain): Ctx {
    return {
      mod: ms, domain, reads: new Set(), writes: new Set(),
      locals: new Map(), loopPool: null, inLoop: false,
    };
  }

  private checkFn(ms: ModuleSym, d: FnDecl): void {
    const ctx = this.ctxFor(ms, 'fn');
    setCurrentFn(ctx, `${ms.ast.name}::${d.name}`);
    ctx.laterLets = collectLetNames(d.body);
    ctx.fnRet = this.resolveType(ms, d.ret);
    ctx.fnHasReturn = false;
    const seen = new Set<string>();
    for (const p of d.params) {
      if (seen.has(p.name)) {
        this.sink.error('FORM-E-102', p.span, `duplicate parameter '${p.name}'`);
      }
      seen.add(p.name);
      ctx.locals.set(p.name, { type: this.resolveType(ms, p.type), letSpan: p.span, assigned: true, isParam: true });
    }
    this.checkStmts(ctx, d.body);
    if (!tAgree(ctx.fnRet!, T.void) && !ctx.fnHasReturn) {
      this.sink.error('FORM-E-310', d.span,
        `fn '${d.name}' returns ${typeName(ctx.fnRet!)} but has no return on some path`);
    }
  }

  private checkStmts(ctx: Ctx, stmts: Stmt[]): void {
    for (const s of stmts) this.checkStmt(ctx, s);
  }

  private checkStmt(ctx: Ctx, s: Stmt): void {
    switch (s.kind) {
      case 'let': {
        if (ctx.locals.has(s.name) && ctx.locals.get(s.name)!.isParam) {
          this.sink.error('FORM-E-102', s.span, `local '${s.name}' shadows a parameter`);
        }
        const expected = s.type ? this.resolveType(ctx.mod, s.type) : null;
        const ty = this.checkExpr(ctx, s.init, expected);
        const final = expected ?? ty;
        if (expected && !tAgree(expected, ty) && !tAgree(ty, T.unknown)) {
          this.sink.error('FORM-E-300', s.span,
            `let '${s.name}' declared ${typeName(expected)} but initializer is ${typeName(ty)}`);
        }
        ctx.locals.set(s.name, { type: final, letSpan: s.span, assigned: true, isParam: false });
        break;
      }
      case 'assign':
        this.checkAssign(ctx, s.target, s.value, s.span);
        break;
      case 'if': {
        const ct = this.checkExpr(ctx, s.cond, T.bool);
        if (!tAgree(ct, T.bool) && !tAgree(ct, T.unknown)) {
          this.sink.error('FORM-E-312', s.cond.span,
            `if condition must be bool, got ${typeName(ct)}`);
        }
        this.checkStmts(ctx, s.then);
        if (s.else) {
          if (Array.isArray(s.else)) this.checkStmts(ctx, s.else);
          else this.checkStmt(ctx, s.else);
        }
        break;
      }
      case 'for':
        this.checkFor(ctx, s);
        break;
      case 'call_stmt':
        this.checkExpr(ctx, s.call, null);
        break;
      case 'spawn':
        this.checkSpawn(ctx, s.pool, s.value, s.span);
        break;
      case 'kill':
        this.checkKill(ctx, s.pool, s.index, s.span);
        break;
      case 'return': {
        const retT = ctx.fnRet ?? T.void;
        if (s.value === null) {
          if (!tAgree(retT, T.void)) {
            this.sink.error('FORM-E-309', s.span,
              `return without a value in a fn returning ${typeName(retT)}`);
          }
        } else {
          const ty = this.checkExpr(ctx, s.value, retT);
          if (!tAgree(retT, ty) && !tAgree(ty, T.unknown) && !tAgree(retT, T.unknown)) {
            this.sink.error('FORM-E-309', s.value.span,
              `return type mismatch: fn returns ${typeName(retT)}, expression is ${typeName(ty)}`);
          }
          if (ctx.fnHasReturn !== undefined) ctx.fnHasReturn = true;
        }
        if (ctx.fnHasReturn !== undefined && s.value !== null) ctx.fnHasReturn = true;
        break;
      }
      case 'apply':
        this.checkApply(ctx, s.applyKind, s.program, s.programSpan, s.args, s.duration, s.span);
        break;
      case 'bad_stmt':
        break; // parser already diagnosed
    }
  }

  /** Single-assignment law + domain write admission. */
  private checkAssign(ctx: Ctx, target: Expr, value: Expr, span: SourceSpan): void {
    // E-303 use-before-let cannot happen for writes; E-302 re-assignment:
    if (target.kind === 'ident') {
      const name = target.name;
      const local = ctx.locals.get(name);
      if (local) {
        this.expressionTypes.set(target, local.type);
        this.sink.error('FORM-E-302', span,
          `'${name}' is let-bound and may not be re-assigned (single-assignment law)`);
        this.checkExpr(ctx, value, local.type);
        return;
      }
      const r = this.resolveUnqualified(ctx.mod, name);
      if (r && r.ambiguous !== true) {
        if (r.sym.kind === 'const') {
          this.expressionTypes.set(target, this.resolveType(r.mod, (r.sym.decl as unknown as ConstDecl).type));
          this.sink.error('FORM-E-333', span, `const '${name}' is not writable`);
          return;
        }
        if (r.sym.kind === 'global') {
          const targetType = this.resolveType(r.mod, (r.sym.decl as unknown as GlobalDecl).type);
          this.expressionTypes.set(target, targetType);
          this.checkExpr(ctx, value, targetType);
          this.writeComponent(ctx, name, span);
          return;
        }
      }
      this.sink.error('FORM-E-203', target.span, `unknown name '${name}'`);
      this.checkExpr(ctx, value, null);
      return;
    }

    // terrain truth mutation (E-460)
    if (target.kind === 'member' && target.obj.kind === 'ident' && target.obj.name === 'terrain') {
      this.sink.error('FORM-E-460', span,
        `direct terrain truth mutation is refused — terrain changes only through applying an @earth field program`);
      this.checkExpr(ctx, value, null);
      return;
    }

    // pool component write: pool.field[index] = value / pool.field = value
    const comp = this.poolComponentOf(ctx, target);
    if (comp) {
      const targetType = this.recordPoolLvalue(ctx, target, comp.pool, comp.fieldType);
      this.checkExpr(ctx, value, targetType);
      this.writeComponent(ctx, comp.component, span);
      return;
    }

    // array element write (globals with array type)
    if (target.kind === 'index') {
      const baseT = this.checkExprRootType(ctx, target.obj);
      this.checkExpr(ctx, value, baseT.t === 'array' ? baseT.elem : null);
      const idxT = this.checkExpr(ctx, target.index, T.u32);
      if (!tAgree(idxT, T.u32) && !tAgree(idxT, T.i32) && !tAgree(idxT, T.unknown)) {
        this.sink.error('FORM-E-305', target.index.span, `array index must be u32/i32, got ${typeName(idxT)}`);
      }
      this.writeComponent(ctx, this.rootNameOf(target), span);
      return;
    }

    this.sink.error('FORM-E-333', span, 'assignment target is not an lvalue');
    this.checkExpr(ctx, value, null);
  }

  /** If `e` addresses a pool component, return its owning column and type. */
  private poolComponentOf(ctx: Ctx, e: Expr): { pool: string; component: string; fieldType: Type } | null {
    const root = this.rootIdentOf(e);
    if (!root) return null;
    const r = this.resolveUnqualified(ctx.mod, root.name);
    if (!r || r.ambiguous === true || r.sym.kind !== 'pool') return null;
    const field = firstMemberAfterRoot(e, root.name);
    if (field === null) return null;
    const struct = this.structOf(r);
    const decl = struct?.fields.find((item) => item.name === field);
    if (!decl) return null;
    return {
      pool: root.name,
      component: `${root.name}.${field}`,
      fieldType: this.resolveType(r.mod, decl.type),
    };
  }

  /** Record an lvalue's exact types without turning a write into a state read. */
  private recordPoolLvalue(ctx: Ctx, e: Expr, pool: string, columnType: Type): Type {
    let type: Type;
    if (e.kind === 'ident') {
      const r = this.resolveUnqualified(ctx.mod, pool);
      const pd = r && r.ambiguous !== true && r.sym.kind === 'pool'
        ? r.sym.decl as unknown as PoolDecl : null;
      type = pd ? { t: 'pool', name: pool, struct: pd.structName } : T.unknown;
    } else if (e.kind === 'member') {
      const base = this.recordPoolLvalue(ctx, e.obj, pool, columnType);
      type = e.obj.kind === 'ident' && e.obj.name === pool
        ? columnType
        : this.memberType(ctx, base, e);
    } else if (e.kind === 'index') {
      const base = this.recordPoolLvalue(ctx, e.obj, pool, columnType);
      const index = this.checkExpr(ctx, e.index, T.u32);
      if (!tAgree(index, T.u32) && !tAgree(index, T.i32) && !tAgree(index, T.unknown)) {
        this.sink.error('FORM-E-305', e.index.span, `pool index must be u32/i32, got ${typeName(index)}`);
      }
      this.checkConstBounds(ctx, e, base);
      type = base.t === 'array' ? base.elem : base;
    } else {
      type = T.unknown;
    }
    this.expressionTypes.set(e, type);
    return type;
  }

  private rootIdentOf(e: Expr): { name: string } | null {
    let cur: Expr = e;
    while (true) {
      if (cur.kind === 'ident') return { name: cur.name };
      if (cur.kind === 'member' || cur.kind === 'index') { cur = cur.obj; continue; }
      return null;
    }
  }

  private rootNameOf(e: Expr): string {
    return this.rootIdentOf(e)?.name ?? '<unknown>';
  }

  /** Type of an lvalue-ish expression without effect-recording side channels. */
  private checkExprRootType(ctx: Ctx, e: Expr): Type {
    return this.checkExpr(ctx, e, null);
  }

  private checkSpawn(ctx: Ctx, pool: string, value: RecordLit, span: SourceSpan): void {
    if (ctx.domain === 'present') {
      this.sink.error('FORM-E-405', span, 'presentation blocks never mutate: spawn is refused (present-purity law)');
    } else if (ctx.domain !== 'sim' && ctx.domain !== 'scenario') {
      this.sink.error('FORM-E-406', span, 'spawn is only admitted in sim systems');
    }
    if (ctx.loopPool) {
      this.sink.error('FORM-E-503', span,
        `spawn inside pool-sugar iteration over '${ctx.loopPool}' is refused (membership mutation, FORM-E-503)`);
    }
    const r = this.resolveUnqualified(ctx.mod, pool);
    if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
      this.sink.error('FORM-E-203', span, `'${pool}' is not a declared pool`);
      return;
    }
    const struct = this.structOf(r);
    if (struct) this.checkRecordLit(ctx, value, struct, r.mod);
    else for (const rf of value.fields) this.checkExpr(ctx, rf.value, null);
    this.writeComponent(ctx, pool, span); // membership change
  }

  private checkKill(ctx: Ctx, pool: string, index: Expr, span: SourceSpan): void {
    if (ctx.domain === 'present') {
      this.sink.error('FORM-E-405', span, 'presentation blocks never mutate: kill is refused (present-purity law)');
    } else if (ctx.domain !== 'sim' && ctx.domain !== 'scenario') {
      this.sink.error('FORM-E-406', span, 'kill is only admitted in sim systems');
    }
    if (ctx.loopPool) {
      this.sink.error('FORM-E-503', span,
        `kill inside pool-sugar iteration over '${ctx.loopPool}' is refused (FORM-E-503)`);
    } else if (ctx.inLoop) {
      this.sink.error('FORM-E-503', span,
        'kill inside an explicit-index loop is refused (stable compaction law, language-semantics §4.4)');
    }
    const r = this.resolveUnqualified(ctx.mod, pool);
    if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
      this.sink.error('FORM-E-203', span, `'${pool}' is not a declared pool`);
      return;
    }
    const it = this.checkExpr(ctx, index, T.u32);
    if (!tAgree(it, T.u32) && !tAgree(it, T.i32) && !tAgree(it, T.unknown)) {
      this.sink.error('FORM-E-305', index.span, `pool index must be u32/i32, got ${typeName(it)}`);
    }
    this.writeComponent(ctx, pool, span);
  }

  // -- for loops --------------------------------------------------------------

  private checkFor(ctx: Ctx, s: Extract<Stmt, { kind: 'for' }>): void {
    const savedLocals = new Map(ctx.locals);
    const savedLoopPool = ctx.loopPool;
    const savedInLoop = ctx.inLoop;

    if (s.range.kind === 'pool') {
      const r = this.resolveUnqualified(ctx.mod, s.range.pool);
      if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
        this.sink.error('FORM-E-203', s.range.poolSpan, `'${s.range.pool}' is not a declared pool`);
      } else {
        const struct = this.structOf(r);
        if (struct) {
          ctx.locals.set(s.varName, { type: { t: 'struct', name: struct.name }, letSpan: s.span, assigned: true, isParam: false });
        } else {
          ctx.locals.set(s.varName, { type: T.unknown, letSpan: s.span, assigned: true, isParam: false });
        }
        this.readComponent(ctx, `${s.range.pool}#members`, s.range.poolSpan);
        ctx.loopPool = s.range.pool;
      }
      ctx.inLoop = true;
      this.checkStmts(ctx, s.body);
      ctx.locals.clear();
      for (const [k, v] of savedLocals) ctx.locals.set(k, v);
      ctx.loopPool = savedLoopPool;
      ctx.inLoop = savedInLoop;
      return;
    }

    // a..b — u32 bounds, ascending, statically bounded (E-501/502)
    const lo = this.checkExpr(ctx, s.range.lo, T.u32);
    const hi = this.checkExpr(ctx, s.range.hi, T.u32);
    for (const [e, t] of [[s.range.lo, lo], [s.range.hi, hi]] as const) {
      if (!tAgree(t, T.u32) && !tAgree(t, T.i32) && !tAgree(t, T.unknown)) {
        this.sink.error('FORM-E-300', e.span, `for range bound must be u32, got ${typeName(t)}`);
      }
    }
    const loV = this.constEval(ctx.mod, s.range.lo);
    const hiV = this.boundEval(ctx, s.range.hi);
    if (loV !== null && hiV !== null) {
      if (loV > hiV) {
        this.sink.error('FORM-E-501', s.span,
          `for range ${loV}..${hiV} is descending or provably empty (ascending only, FORM-E-501)`);
      }
    } else if (hiV === null && !this.isPoolCount(ctx, s.range.hi)) {
      this.sink.error('FORM-E-502', s.range.hi.span,
        'for trip count is not statically bounded (bounds must be constants or pool.count of known capacity, FORM-E-502)');
    }
    ctx.locals.set(s.varName, { type: T.u32, letSpan: s.span, assigned: true, isParam: false });
    ctx.inLoop = true;
    this.checkStmts(ctx, s.body);
    ctx.locals.clear();
    for (const [k, v] of savedLocals) ctx.locals.set(k, v);
    ctx.loopPool = savedLoopPool;
    ctx.inLoop = savedInLoop;
  }

  /** `p.count` of a declared pool is a legal dynamic bound (§4.4). */
  private isPoolCount(ctx: Ctx, e: Expr): boolean {
    return e.kind === 'member' && e.field === 'count' && e.obj.kind === 'ident'
      ? (() => {
          const r = this.resolveUnqualified(ctx.mod, e.obj.name);
          return !!r && r.ambiguous !== true && r.sym.kind === 'pool';
        })()
      : false;
  }

  /** Constant bound, or pool.count -> capacity constant. */
  private boundEval(ctx: Ctx, e: Expr): bigint | null {
    const v = this.constEval(ctx.mod, e);
    if (v !== null) return v;
    if (this.isPoolCount(ctx, e)) {
      const r = this.resolveUnqualified(ctx.mod, (e as { obj: { name: string } }).obj.name);
      if (r && r.ambiguous !== true && r.sym.kind === 'pool') {
        const pd = r.sym.decl as unknown as PoolDecl;
        if (pd.capacity?.kind === 'int') return pd.capacity.value;
        if (pd.capacity?.kind === 'name') {
          const cap = r.mod.table.get(pd.capacity.name);
          if (cap && cap.kind === 'const') {
            const v = this.constEval(r.mod, (cap.decl as unknown as ConstDecl).init);
            if (v !== null) return v;
          }
        }
      }
    }
    return null;
  }

  // -- apply (§6.4) -------------------------------------------------------------

  private checkApply(
    ctx: Ctx, applyKind: string, program: string, programSpan: SourceSpan,
    args: { name: string; value: Expr; span: SourceSpan }[],
    duration: Expr, span: SourceSpan,
  ): void {
    const r = this.resolveUnqualified(ctx.mod, program);
    const field = r && r.ambiguous !== true && r.sym.kind === 'field' ? (r.sym.decl as unknown as FieldDecl) : null;
    if (!field) {
      this.sink.error('FORM-E-203', programSpan, `'${program}' is not a declared field program`);
      return;
    }
    if (applyKind === 'terrain_field') {
      if (field.profile !== 'earth') {
        this.sink.error('FORM-E-462', programSpan,
          `applied field program '${program}' is @${field.profile}, not @earth (FORM-E-462)`);
      }
      if (field.footprint.kind === 'none') {
        this.sink.error('FORM-E-463', programSpan,
          'earth application of a program without a footprint (FORM-E-463)');
      }
    } else {
      // apply flow prog(pool: p, params: ...) — pool mapping laws (E-664/667)
      if (field.profile !== 'flow') {
        this.sink.error('FORM-E-667', programSpan,
          `'apply flow' on @${field.profile} program '${program}' (FORM-E-667)`);
      }
      const poolArg = args.find((a) => a.name === 'pool');
      if (!poolArg || poolArg.value.kind !== 'ident') {
        this.sink.error('FORM-E-667', span, "'apply flow' requires a pool: argument naming its mapped pool");
      } else {
        const poolName = poolArg.value.name;
        const pool = this.resolveUnqualified(ctx.mod, poolName);
        const pd = pool && pool.ambiguous !== true && pool.sym.kind === 'pool'
          ? pool.sym.decl as unknown as PoolDecl : null;
        this.expressionTypes.set(poolArg.value, pd
          ? { t: 'pool', name: poolName, struct: pd.structName }
          : T.unknown);
        this.checkFlowPoolMapping(ctx, poolName, poolArg.value.span);
        this.writeComponent(ctx, poolName, poolArg.value.span);
      }
    }
    // required arguments
    const need = applyKind === 'terrain_field' ? ['origin'] : ['pool'];
    for (const n of need) {
      if (!args.some((a) => a.name === n)) {
        this.sink.error('FORM-E-463', span, `apply is missing its '${n}' argument (FORM-E-463)`);
      }
    }
    for (const a of args) {
      if (a.name === 'origin') this.checkExpr(ctx, a.value, T.world2);
      else if (a.name === 'params') {
        const pt = field.paramsStruct ? { t: 'struct', name: field.paramsStruct } as Type : null;
        this.checkExpr(ctx, a.value, pt);
      } else if (a.name !== 'pool') {
        this.sink.error('FORM-E-602', a.span, `unknown apply argument '${a.name}'`);
      }
    }
    const dur = this.constEval(ctx.mod, duration);
    if (dur === null) {
      this.sink.error('FORM-E-308', duration.span, 'apply duration must be a constant tick count');
    } else if (dur <= 0n) {
      this.sink.error('FORM-E-463', duration.span, `apply duration must be positive (got ${dur})`);
    }
    const durationType = this.checkExpr(ctx, duration, T.u32);
    if (!tAgree(durationType, T.u32) && !tAgree(durationType, T.i32) && !tAgree(durationType, T.unknown)) {
      this.sink.error('FORM-E-300', duration.span, `apply duration must be u32 ticks, got ${typeName(durationType)}`);
    }
    // applying terrain is a terrain write
    if (applyKind === 'terrain_field') this.writeComponent(ctx, 'terrain', span);
  }

  /** flow lane mapping: position/velocity/age (+ optional representation). */
  private checkFlowPoolMapping(ctx: Ctx, pool: string, span: SourceSpan): void {
    const r = this.resolveUnqualified(ctx.mod, pool);
    if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
      this.sink.error('FORM-E-667', span, `'${pool}' is not a pool (flow programs apply to their mapped pool)`);
      return;
    }
    const struct = this.structOf(r);
    if (!struct) return;
    const has = (n: string) => struct.fields.some((f) => f.name === n);
    const ok = has('position') && has('velocity') && has('age');
    if (!ok) {
      this.sink.error('FORM-E-664', span,
        `pool '${pool}' struct '${struct.name}' does not match the flow lane mapping (needs position: world3, velocity: velocity3, age: u32)`);
    }
  }

  private structOf(r: { sym: Sym; mod: ModuleSym; ambiguous: boolean } | null): StructDecl | null {
    if (!r || r.ambiguous === true) return null;
    if (r.sym.kind === 'struct') return r.sym.decl as unknown as StructDecl;
    if (r.sym.kind === 'pool') {
      const pd = r.sym.decl as unknown as PoolDecl;
      const s = r.mod.table.get(pd.structName);
      return s && s.kind === 'struct' ? (s.decl as unknown as StructDecl) : null;
    }
    return null;
  }

  // -- read/write admission ------------------------------------------------------

  /** `declared` covers `used` when equal or a whole-pool prefix of it. */
  private covers(declared: string, used: string): boolean {
    if (declared === used) return true;
    // whole-pool declaration covers field components and pool membership
    if (!declared.includes('.') && !declared.includes('#')
        && (used.startsWith(declared + '.') || used === `${declared}#members`)) return true;
    return false;
  }

  private readComponent(ctx: Ctx, component: string, span: SourceSpan): void {
    if (ctx.domain === 'present' || ctx.domain === 'scenario' || ctx.domain === 'field') return;
    if (ctx.domain === 'fn') {
      this.sink.error('FORM-E-402', span,
        `fn bodies are pure — read of state '${component}' requires a reads declaration in a system (FORM-E-402)`);
      return;
    }
    for (const d of ctx.reads) if (this.covers(d, component)) return;
    this.sink.error('FORM-E-402', span,
      `system reads '${component}' without declaring it in its reads list (FORM-E-402)`);
  }

  private writeComponent(ctx: Ctx, component: string, span: SourceSpan): void {
    if (ctx.domain === 'present') {
      this.sink.error('FORM-E-405', span,
        `presentation blocks never mutate — write of '${component}' (present-purity law, FORM-E-405)`);
      return;
    }
    if (ctx.domain === 'fn') {
      this.sink.error('FORM-E-400', span,
        `write to truth state '${component}' outside a system (fn bodies are pure, FORM-E-400)`);
      return;
    }
    if (ctx.domain === 'field') {
      this.sink.error('FORM-E-656', span,
        `state access '${component}' inside a field body (field programs read only their input record, FORM-E-656)`);
      return;
    }
    if (ctx.domain === 'scenario') return; // test drivers drive systems, not state
    for (const d of ctx.writes) if (this.covers(d, component)) return;
    this.sink.error('FORM-E-401', span,
      `system writes '${component}' without declaring it in its writes list (FORM-E-401)`);
  }

  // -- expressions ---------------------------------------------------------------

  private checkExpr(ctx: Ctx, e: Expr, expected: Type | null): Type {
    const type = this.checkExprInner(ctx, e, expected);
    this.expressionTypes.set(e, type);
    return type;
  }

  private checkExprInner(ctx: Ctx, e: Expr, expected: Type | null): Type {
    switch (e.kind) {
      case 'literal':
        return this.checkLiteral(ctx, e, expected);
      case 'string':
        this.sink.error('FORM-E-705', e.span,
          'strings exist only in import paths, sound.sample references, scenario.load targets and capture names — there is no string type (FORM-E-705)');
        return T.unknown;
      case 'ident':
        return this.checkIdent(ctx, e, expected);
      case 'member':
        return this.checkMember(ctx, e, expected);
      case 'index':
        return this.checkIndex(ctx, e, expected);
      case 'call':
        return this.checkCall(ctx, e, expected);
      case 'unary':
        return this.checkUnary(ctx, e, expected);
      case 'binary':
        return this.checkBinary(ctx, e, expected);
      case 'if_expr': {
        const ct = this.checkExpr(ctx, e.cond, T.bool);
        if (!tAgree(ct, T.bool) && !tAgree(ct, T.unknown)) {
          this.sink.error('FORM-E-312', e.cond.span, `if condition must be bool, got ${typeName(ct)}`);
        }
        let a = this.checkExpr(ctx, e.then, expected);
        let b = this.checkExpr(ctx, e.else, expected);
        if (expected === null) {
          if (isBareInt(e.then) && adoptableLiteralType(b)) {
            a = this.checkExpr(ctx, e.then, b);
          } else if (isBareInt(e.else) && adoptableLiteralType(a)) {
            b = this.checkExpr(ctx, e.else, a);
          }
        }
        if (!tAgree(a, b) && !tAgree(a, T.unknown) && !tAgree(b, T.unknown)) {
          this.sink.error('FORM-E-311', e.span,
            `if select-expression branches have different types (${typeName(a)} vs ${typeName(b)})`);
          return a;
        }
        return tAgree(a, T.unknown) ? b : a;
      }
      case 'record':
        return this.checkFreeRecordLit(ctx, e);
      case 'range':
        this.sink.error('FORM-E-107', e.span, "'..' is only admitted in a for header (FORM-E-107)");
        return T.unknown;
    }
  }

  private checkIdent(ctx: Ctx, e: Extract<Expr, { kind: 'ident' }>, _expected: Type | null): Type {
    const name = e.name;
    const local = ctx.locals.get(name);
    if (local) return local.type;

    const r = this.resolveUnqualified(ctx.mod, name);
    if (r === null) {
      if (ctx.laterLets?.has(name)) {
        this.sink.error('FORM-E-303', e.span,
          `'${name}' is used before its let (single-assignment locals are use-after-let only, FORM-E-303)`);
        return T.unknown;
      }
      const code = ctx.inAssert ? 'FORM-E-906' : 'FORM-E-203';
      this.sink.error(code, e.span,
        ctx.inAssert
          ? `scenario assertion references undeclared state '${name}' (FORM-E-906)`
          : `unknown name '${name}' (FORM-E-203)`);
      return T.unknown;
    }
    if (r.ambiguous === true) {
      this.sink.error('FORM-E-205', e.span, `ambiguous unqualified name '${name}' (FORM-E-205)`);
      return T.unknown;
    }
    const { sym, mod } = r;
    switch (sym.kind) {
      case 'const': {
        const cd = sym.decl as unknown as ConstDecl;
        return this.resolveType(mod, cd.type);
      }
      case 'enum': {
        // bare enum name: only legal as the object of `Enum.member`
        this.sink.error('FORM-E-307', e.span,
          `enum '${name}' used without a member (write ${name}.<member>)`);
        return { t: 'enum', name };
      }
      case 'struct':
        this.sink.error('FORM-E-110', e.span, `struct type '${name}' used as a value`);
        return T.unknown;
      case 'pool': {
        // a bare pool value defers its read to the precise component
        // (pool.field via member/index, #members via pool sugar / spawn)
        const pd = sym.decl as unknown as PoolDecl;
        return { t: 'pool', name, struct: pd.structName };
      }
      case 'global': {
        this.readComponent(ctx, name, e.span);
        const gd = sym.decl as unknown as GlobalDecl;
        return this.resolveType(mod, gd.type);
      }
      case 'fn':
        this.sink.error('FORM-E-706', e.span,
          `function '${name}' used as a value — first-class functions are refused in L1 (FORM-E-706)`);
        return T.unknown;
      case 'system':
        this.sink.error('FORM-E-706', e.span,
          `system '${name}' is not a value (systems run on the compile-time schedule)`);
        return T.unknown;
      case 'field':
        this.sink.error('FORM-E-706', e.span,
          `field program '${name}' is not a value (apply it with 'apply', or per element for flow)`);
        return T.unknown;
      case 'sound':
        this.sink.error('FORM-E-706', e.span, `sound '${name}' is not a value (reference it from an audio emit)`);
        return T.sound;
      default:
        this.sink.error('FORM-E-203', e.span, `'${name}' is not a value`);
        return T.unknown;
    }
  }

  private checkMember(ctx: Ctx, e: Extract<Expr, { kind: 'member' }>, _expected: Type | null): Type {
    // qualified import: m.name
    if (e.obj.kind === 'ident' && this.byName.has(e.obj.name) && !ctx.locals.has(e.obj.name)
        && !this.resolveUnqualified(ctx.mod, e.obj.name)) {
      const target = this.byName.get(e.obj.name)!;
      const sym = target.table.get(e.field);
      if (!sym) {
        this.sink.error('FORM-E-203', e.span, `module '${e.obj.name}' has no name '${e.field}'`);
        return T.unknown;
      }
      return this.symValueType(sym, target, e);
    }
    // enum member
    if (e.obj.kind === 'ident' && !ctx.locals.has(e.obj.name)) {
      const r = this.resolveUnqualified(ctx.mod, e.obj.name);
      if (r && r.ambiguous !== true && r.sym.kind === 'enum') {
        const ed = r.sym.decl as unknown as EnumDecl;
        if (!ed.members.some((m) => m.name === e.field)) {
          this.sink.error('FORM-E-307', e.span,
            `enum '${e.obj.name}' has no member '${e.field}'`);
        }
        return { t: 'enum', name: e.obj.name };
      }
    }
    // pool.count
    if (e.obj.kind === 'ident') {
      const r = this.resolveUnqualified(ctx.mod, e.obj.name);
      if (r && r.ambiguous !== true && r.sym.kind === 'pool' && e.field === 'count') {
        this.readComponent(ctx, e.obj.name, e.span);
        return T.u32;
      }
    }
    // terrain.height
    if (e.obj.kind === 'ident' && e.obj.name === 'terrain' && e.field === 'height') {
      this.sink.error('FORM-E-110', e.span,
        "terrain.height is an intrinsic call — write terrain.height(world2)");
      return T.unknown;
    }
    const objT = this.checkExpr(ctx, e.obj, null);
    return this.memberType(ctx, objT, e);
  }

  private symValueType(sym: Sym, mod: ModuleSym, e: Extract<Expr, { kind: 'member' }>): Type {
    switch (sym.kind) {
      case 'const': return this.resolveType(mod, (sym.decl as unknown as ConstDecl).type);
      case 'enum': return { t: 'enum', name: e.field };
      case 'global': return this.resolveType(mod, (sym.decl as unknown as GlobalDecl).type);
      default:
        this.sink.error('FORM-E-203', e.span, `'${e.field}' is not a value`);
        return T.unknown;
    }
  }

  /** Field-dialect input lanes (language-semantics §6.2). */
  private static readonly FIELD_LANES: Record<string, Record<string, Type>> = {
    __earth_sample: { x: T.fx16, z: T.fx16, age: T.u32, phase: T.fx16 },
    __flow_p: { x: T.fx16, y: T.fx16, z: T.fx16, vx: T.fx16, vy: T.fx16, vz: T.fx16, age: T.u32, seed: T.u32, dt: T.fx16 },
  };

  private memberType(ctx: Ctx, objT: Type, e: Extract<Expr, { kind: 'member' }>): Type {
    if (objT.t === 'world2' || objT.t === 'world3' || objT.t === 'velocity3') {
      if (['x', 'y', 'z'].includes(e.field)) {
        if (objT.t === 'world2' && e.field === 'z') {
          this.sink.error('FORM-E-306', e.span, "world2 has no '.z' component");
          return T.unknown;
        }
        return T.fx24;
      }
      this.sink.error('FORM-E-306', e.span, `vector type ${objT.t} has no component '${e.field}'`);
      return T.unknown;
    }
    if (objT.t === 'struct') {
      const lanes = Checker.FIELD_LANES[objT.name];
      if (lanes) {
        const lt = lanes[e.field];
        if (!lt) {
          this.sink.error('FORM-E-306', e.span,
            `input record '${objT.name.replace('__', '')}' has no lane '${e.field}'`);
          return T.unknown;
        }
        return lt;
      }
      const sd = this.findStruct(ctx, objT.name);
      if (!sd) return T.unknown;
      const f = sd.fields.find((x) => x.name === e.field);
      if (!f) {
        this.sink.error('FORM-E-306', e.span,
          `struct '${objT.name}' has no field '${e.field}'`);
        return T.unknown;
      }
      return this.resolveType(ctx.mod, f.type);
    }
    if (objT.t === 'pool') {
      // pool.field — the SoA column (used with [i])
      const sd = this.findStructIn(ctx, objT.struct);
      if (!sd) return T.unknown;
      const f = sd.fields.find((x) => x.name === e.field);
      if (!f) {
        this.sink.error('FORM-E-306', e.span,
          `pool '${objT.name}' struct '${objT.struct}' has no field '${e.field}'`);
        return T.unknown;
      }
      this.readComponent(ctx, `${objT.name}.${e.field}`, e.span);
      return this.resolveType(ctx.mod, f.type);
    }
    if (objT.t === 'padframe' || objT.t === 'unknown') return T.unknown;
    this.sink.error('FORM-E-306', e.span,
      `field access '.${e.field}' on non-struct type ${typeName(objT)}`);
    return T.unknown;
  }

  private findStruct(ctx: Ctx, name: string): StructDecl | null {
    const r = this.resolveUnqualified(ctx.mod, name);
    if (r && r.ambiguous !== true && r.sym.kind === 'struct') {
      return r.sym.decl as unknown as StructDecl;
    }
    for (const m of this.mods) {
      for (const d of m.ast.decls) {
        if (d.kind === 'Struct' && d.name === name) return d;
      }
    }
    return null;
  }

  private findStructIn(ctx: Ctx, name: string): StructDecl | null {
    return this.findStruct(ctx, name);
  }

  private checkIndex(ctx: Ctx, e: Extract<Expr, { kind: 'index' }>, expected: Type | null): Type {
    // pool column access: pool.field[i]
    if (e.obj.kind === 'member') {
      const colT = this.checkExpr(ctx, e.obj, null); // records the read
      const it = this.checkExpr(ctx, e.index, T.u32);
      if (!tAgree(it, T.u32) && !tAgree(it, T.i32) && !tAgree(it, T.unknown)) {
        this.sink.error('FORM-E-305', e.index.span, `pool index must be u32/i32, got ${typeName(it)}`);
      }
      this.checkConstBounds(ctx, e, colT);
      return colT;
    }
    const objT = this.checkExpr(ctx, e.obj, null);
    if (objT.t === 'array') {
      const it = this.checkExpr(ctx, e.index, T.u32);
      if (!tAgree(it, T.u32) && !tAgree(it, T.i32) && !tAgree(it, T.unknown)) {
        this.sink.error('FORM-E-305', e.index.span, `array index must be u32/i32, got ${typeName(it)}`);
      }
      const idx = this.constEval(ctx.mod, e.index);
      if (idx !== null && (idx < 0n || idx >= BigInt(objT.len))) {
        this.sink.error('FORM-E-820', e.span,
          `compile-time-provable out-of-bounds index ${idx} (length ${objT.len})`);
      }
      return objT.elem;
    }
    if (objT.t === 'unknown') {
      this.checkExpr(ctx, e.index, null);
      return T.unknown;
    }
    this.checkExpr(ctx, e.index, null);
    this.sink.error('FORM-E-300', e.span, `indexing non-array type ${typeName(objT)}`);
    void expected;
    return T.unknown;
  }

  /** E-820: constant index against pool capacity. */
  private checkConstBounds(ctx: Ctx, e: Extract<Expr, { kind: 'index' }>, colT: Type): void {
    const idx = this.constEval(ctx.mod, e.index);
    if (idx === null) return;
    const poolName = e.obj.kind === 'member' && e.obj.obj.kind === 'ident' ? e.obj.obj.name : null;
    if (!poolName) return;
    const r = this.resolveUnqualified(ctx.mod, poolName);
    if (!r || r.ambiguous === true || r.sym.kind !== 'pool') return;
    const pd = r.sym.decl as unknown as PoolDecl;
    const cap = pd.capacity?.kind === 'int' ? pd.capacity.value
      : pd.capacity ? this.constEval(r.mod, { kind: 'ident', name: pd.capacity.name, span: pd.span } as Expr) : null;
    if (cap !== null && idx >= cap) {
      this.sink.error('FORM-E-820', e.span,
        `compile-time-provable pool index ${idx} out of bounds (capacity ${cap})`);
    }
    void colT;
  }

  private checkUnary(ctx: Ctx, e: Extract<Expr, { kind: 'unary' }>, expected: Type | null): Type {
    // negative literal to u32 (E-320)
    if (e.op === '-' && e.operand.kind === 'literal' && e.operand.lit === 'int'
        && expected && tAgree(expected, T.u32)) {
      this.sink.error('FORM-E-320', e.span, 'negative literal assigned to u32 (FORM-E-320)');
      return T.u32;
    }
    const t = this.checkExpr(ctx, e.operand, e.op === '-' ? expected : null);
    if (tAgree(t, T.unknown)) return T.unknown;
    switch (e.op) {
      case '-':
        if (!NUMERIC.has(t.t)) {
          this.sink.error('FORM-E-300', e.span, `unary '-' on ${typeName(t)}`);
          return T.unknown;
        }
        return t;
      case '!':
        if (!tAgree(t, T.bool)) {
          this.sink.error('FORM-E-300', e.span, `unary '!' on ${typeName(t)} (bool expected)`);
          return T.unknown;
        }
        return T.bool;
      case '~':
        if (!INTS.has(t.t)) {
          this.sink.error('FORM-E-300', e.span, `unary '~' on ${typeName(t)} (integer expected)`);
          return T.unknown;
        }
        return t;
    }
  }

  private checkBinary(ctx: Ctx, e: Extract<Expr, { kind: 'binary' }>, expected: Type | null): Type {
    const op = e.op;
    // dialect refusals first (E-665): / % bitwise && || in field bodies
    if (ctx.domain === 'field' && ['/', '%', '&', '|', '^', '<<', '>>', '&&', '||'].includes(op)) {
      this.sink.error('FORM-E-665', e.span,
        `operator '${op}' is not admitted in the field dialect (§6.3 surface; use rcp, comparisons feed SELECT)`);
    }
    const cmp = ['<', '<=', '>', '>=', '==', '!='].includes(op);
    const operandExpected = op === '&&' || op === '||' ? T.bool : cmp ? null : expected;
    let l = this.checkExpr(ctx, e.l, isBareInt(e.l) ? operandExpected : null);
    let r = this.checkExpr(ctx, e.r, isBareInt(e.r) ? operandExpected : null);
    // target-typing: a bare int literal adopts the other operand's type
    // (§1.2 literals are typed by context; no implicit conversions otherwise)
    if (isBareInt(e.l) && adoptableLiteralType(r)) {
      l = this.checkExpr(ctx, e.l, r);
    } else if (isBareInt(e.r) && adoptableLiteralType(l)) {
      r = this.checkExpr(ctx, e.r, l);
    }

    // space-typing (E-330) and mixed precision (E-331)
    const lV = VECTORS.has(l.t), rV = VECTORS.has(r.t);
    if (lV || rV) {
      if (lV && rV) {
        if (l.t === 'world3' && r.t === 'velocity3' || l.t === 'velocity3' && r.t === 'world3') {
          this.sink.error('FORM-E-330', e.span,
            `world3 and velocity3 cannot mix in one operator (space-typing, FORM-E-330; convert explicitly)`);
          return T.unknown;
        }
        if (l.t !== r.t) {
          this.sink.error('FORM-E-300', e.span, `${op} on ${typeName(l)} and ${typeName(r)}`);
          return T.unknown;
        }
        if (op === '+' || op === '-') return l;
        this.sink.error('FORM-E-300', e.span, `operator '${op}' on vector type ${typeName(l)}`);
        return T.unknown;
      }
      this.sink.error('FORM-E-300', e.span,
        `${op} on ${typeName(l)} and ${typeName(r)} (vectors combine with vectors only)`);
      return T.unknown;
    }
    if ((l.t === 'fx24' && r.t === 'fx16') || (l.t === 'fx16' && r.t === 'fx24')
        || ((l.t === 'world2' || l.t === 'world3') && r.t === 'fx16') || (l.t === 'fx16' && (r.t === 'world2' || r.t === 'world3'))) {
      this.sink.error('FORM-E-331', e.span,
        `mixed-precision operands (${typeName(l)} vs ${typeName(r)}) — convert explicitly with a named intrinsic (FORM-E-331)`);
      return T.unknown;
    }

    if (cmp) {
      if (!tAgree(l, r) && !tAgree(l, T.unknown) && !tAgree(r, T.unknown)) {
        this.sink.error('FORM-E-300', e.span,
          `comparison of ${typeName(l)} and ${typeName(r)} (both operands must have the same type)`);
      }
      return T.bool;
    }
    if (op === '&&' || op === '||') {
      if (!tAgree(l, T.bool)) this.sink.error('FORM-E-300', e.l.span, `'${op}' on ${typeName(l)} (bool expected)`);
      if (!tAgree(r, T.bool)) this.sink.error('FORM-E-300', e.r.span, `'${op}' on ${typeName(r)} (bool expected)`);
      return T.bool;
    }
    // arithmetic / bitwise
    if (!tAgree(l, r)) {
      this.sink.error('FORM-E-300', e.span,
        `'${op}' on ${typeName(l)} and ${typeName(r)} (both operands must have the same type, no implicit conversions)`);
      return T.unknown;
    }
    if (INTS.has(l.t) || l.t === 'fx16' || l.t === 'fx24') {
      if (['<<', '>>', '&', '|', '^'].includes(op) && !INTS.has(l.t)) {
        this.sink.error('FORM-E-300', e.span, `bitwise operator '${op}' on ${typeName(l)} (integer expected)`);
        return T.unknown;
      }
      return l;
    }
    if (l.t === 'angle16' && (op === '+' || op === '-')) return l;
    if (l.t === 'unit8' && op === '*') return l;
    if (l.t === 'bool' && ['&', '|', '^'].includes(op)) return T.bool;
    this.sink.error('FORM-E-300', e.span, `operator '${op}' on ${typeName(l)}`);
    return T.unknown;
  }

  // -- literals (target-typed, §1.2) -------------------------------------------

  private checkLiteral(ctx: Ctx, e: Extract<Expr, { kind: 'literal' }>, expected: Type | null): Type {
    ctxInTolerance = ctx.inTolerance === true;
    if (e.lit === 'bool') return T.bool;
    if (e.lit === 'colour') {
      if (expected && !tAgree(expected, T.colour8) && !tAgree(expected, T.unknown)) {
        this.sink.error('FORM-E-313', e.span,
          `colour literal where ${typeName(expected)} is expected (FORM-E-313)`);
      }
      return T.colour8;
    }
    if (e.lit === 'tick') {
      if (expected && !tAgree(expected, T.u32) && !tAgree(expected, T.i32) && !tAgree(expected, T.unknown)) {
        this.sink.error('FORM-E-313', e.span, `tick literal where ${typeName(expected)} is expected (FORM-E-313)`);
      }
      if ((e.intVal ?? 0n) > 0xffffffffn) {
        this.sink.error('FORM-E-007', e.span, `tick literal ${e.text} exceeds u32`);
      }
      return T.u32;
    }
    if (e.lit === 'int') {
      const v = e.intVal!;
      const target = expected && (tAgree(expected, T.i32) || tAgree(expected, T.u32)) ? expected
        : expected && (tAgree(expected, T.fx16) || tAgree(expected, T.fx24)) ? expected : null;
      if (target && tAgree(target, T.u32)) {
        if (v > 0xffffffffn) this.sink.error('FORM-E-007', e.span, `integer ${v} exceeds u32`);
      } else if (target && tAgree(target, T.i32)) {
        if (v > 0x7fffffffn) this.sink.error('FORM-E-007', e.span, `integer ${v} exceeds i32`);
      } else if (target && tAgree(target, T.fx16)) {
        if (v >= 32768n) this.sink.error('FORM-E-007', e.span, `integer ${v} exceeds fx16 range (±32768)`);
      } else if (target && tAgree(target, T.fx24)) {
        if (v >= 8192n) this.sink.error('FORM-E-007', e.span, `integer ${v} exceeds fx24 range (±8192)`);
      }
      return target ?? T.i32;
    }
    // fractional
    const f = e.frac!;
    const want = (t: Type) => expected === null || tAgree(expected, t) || tAgree(expected, T.unknown);
    switch (f.suffix) {
      case '': case 'm': case 'px': {
        if (!want(T.fx16)) {
          this.sink.error('FORM-E-313', e.span,
            `fx16 literal '${e.text}' where ${typeName(expected!)} is expected (FORM-E-313)`);
        }
        this.checkFracExact(e, 16n, -32768n, 32768n);
        return T.fx16;
      }
      case 'w': {
        if (!want(T.fx24)) {
          this.sink.error('FORM-E-313', e.span,
            `fx24 literal '${e.text}' where ${typeName(expected!)} is expected (FORM-E-313)`);
        }
        this.checkFracExact(e, 24n, -8192n, 8192n);
        return T.fx24;
      }
      case 'turn': case 'deg': {
        if (!want(T.angle16)) {
          this.sink.error('FORM-E-313', e.span,
            `angle16 literal '${e.text}' where ${typeName(expected!)} is expected (FORM-E-313)`);
        }
        // exactness: value_in_turns * 2^16 must be an integer; wraps mod 1
        const num = BigInt(f.intDigits + (f.fracDigits ? f.fracDigits : ''));
        const den = BigInt(10) ** BigInt(f.fracDigits.length || 0) * (f.suffix === 'deg' ? 360n : 1n);
        if ((num << 16n) % den !== 0n) {
          this.sink.error('FORM-E-008', e.span,
            `'${e.text}' is not exactly representable in U 0.0.16 turns (FORM-E-008)`);
        }
        return T.angle16;
      }
      case '%': {
        if (!want(T.unit8)) {
          this.sink.error('FORM-E-313', e.span,
            `unit8 literal '${e.text}' where ${typeName(expected!)} is expected (FORM-E-313)`);
        }
        const pct = BigInt(f.intDigits);
        if (pct < 0n) this.sink.error('FORM-E-007', e.span, `negative percent '${e.text}'`);
        return T.unit8;
      }
    }
    return T.unknown;
  }

  /** Q-format exactness + range (§1.2; E-007/E-008). */
  private checkFracExact(e: Extract<Expr, { kind: 'literal' }>, bits: bigint, lo: bigint, hi: bigint): void {
    const f = e.frac!;
    const num = BigInt(f.intDigits + (f.fracDigits ? f.fracDigits : ''));
    const den = 10n ** BigInt(f.fracDigits.length || 0);
    const raw = (num << bits) / den; // for the range check
    const exact = (num << bits) % den === 0n;
    if (!exact) {
      this.sink.error(ctxInTolerance ? 'FORM-E-907' : 'FORM-E-008', e.span,
        ctxInTolerance
          ? `tolerance '${e.text}' is not exactly representable as fx16 (FORM-E-907, §1.2 law)`
          : `'${e.text}' is not exactly representable in this Q format (2^-${bits} steps; FORM-E-008)`);
      return;
    }
    const value = Number(num) / Number(den);
    if (value <= lo || value >= hi) {
      void raw;
      this.sink.error('FORM-E-007', e.span,
        `'${e.text}' exceeds the Q format range [${lo}, ${hi}) (FORM-E-007)`);
    }
  }

  // -- record literals ------------------------------------------------------------

  private checkRecordLit(ctx: Ctx, rec: RecordLit, struct: StructDecl, mod: ModuleSym): void {
    this.expressionTypes.set(rec, { t: 'struct', name: struct.name });
    const seen = new Set<string>();
    for (const rf of rec.fields) {
      if (!struct.fields.some((f) => f.name === rf.name)) {
        this.sink.error('FORM-E-104', rf.span,
          `record literal names field '${rf.name}' which struct '${struct.name}' does not have (FORM-E-104)`);
        this.checkExpr(ctx, rf.value, null);
        continue;
      }
      if (seen.has(rf.name)) {
        this.sink.error('FORM-E-106', rf.span, `duplicate field '${rf.name}' in record literal (FORM-E-106)`);
      }
      seen.add(rf.name);
      const f = struct.fields.find((x) => x.name === rf.name)!;
      const want = this.resolveType(mod, f.type);
      this.checkExpr(ctx, rf.value, want);
    }
    for (const f of struct.fields) {
      if (!seen.has(f.name)) {
        this.sink.error('FORM-E-105', rec.span,
          `record literal omits field '${f.name}' of struct '${struct.name}' (all fields required, FORM-E-105)`);
      }
    }
  }

  /**
   * Vector record literals (world2/world3/velocity3) — the §1.2 literal table
   * has no vector literal, yet `global x: world3 = <const_expr>;` is the only
   * grammar. W3.2 resolution (spec-issue note): `world3 { x = 1m, ... }` etc.,
   * component types per §3.2 (fx24 lanes).
   */
  private static readonly VECTOR_SHAPE: Record<string, { fields: string[]; elem: Type }> = {
    world2: { fields: ['x', 'y'], elem: T.fx24 },
    world3: { fields: ['x', 'y', 'z'], elem: T.fx24 },
    velocity3: { fields: ['x', 'y', 'z'], elem: T.fx24 },
  };

  private checkFreeRecordLit(ctx: Ctx, rec: RecordLit): Type {
    const vec = Checker.VECTOR_SHAPE[rec.typeName];
    if (vec) {
      const seen = new Set<string>();
      for (const f of rec.fields) {
        if (!vec.fields.includes(f.name)) {
          this.sink.error('FORM-E-104', f.span,
            `vector type '${rec.typeName}' has no component '${f.name}' (${vec.fields.join('/')})`);
          continue;
        }
        if (seen.has(f.name)) this.sink.error('FORM-E-106', f.span, `duplicate component '${f.name}' (FORM-E-106)`);
        seen.add(f.name);
        this.checkExpr(ctx, f.value, vec.elem);
      }
      for (const name of vec.fields) {
        if (!seen.has(name)) {
          this.sink.error('FORM-E-105', rec.span,
            `vector literal '${rec.typeName}' omits component '${name}' (FORM-E-105)`);
        }
      }
      return { t: rec.typeName as 'world2' | 'world3' | 'velocity3' };
    }
    const r = this.resolveUnqualified(ctx.mod, rec.typeName);
    const struct = r && r.ambiguous !== true && r.sym.kind === 'struct'
      ? (r.sym.decl as unknown as StructDecl) : null;
    if (struct) {
      this.checkRecordLit(ctx, rec, struct, r!.mod);
      return { t: 'struct', name: rec.typeName };
    }
    this.sink.error('FORM-E-301', rec.span, `record literal of unknown struct '${rec.typeName}'`);
    for (const rf of rec.fields) this.checkExpr(ctx, rf.value, null);
    return T.unknown;
  }

  // ---------------------------------------------------------------------------
  // Constant evaluation (const exprs: literals, consts, enum members, arithmetic)
  // ---------------------------------------------------------------------------

  private isConstExpr(e: Expr): boolean {
    switch (e.kind) {
      case 'literal': return true;
      case 'unary': return this.isConstExpr(e.operand);
      case 'binary': return this.isConstExpr(e.l) && this.isConstExpr(e.r);
      case 'record': return true; // vector/struct record literal
      case 'ident': return true; // resolution is constEval's job
      case 'member': return true; // enum member / module const
      default: return false;
    }
  }

  /** Integer constant evaluation; null when not a known constant. */
  private constEval(ms: ModuleSym, e: Expr): bigint | null {
    switch (e.kind) {
      case 'literal':
        return e.lit === 'int' || e.lit === 'tick' ? e.intVal ?? null : null;
      case 'unary': {
        if (e.op !== '-') return null;
        const v = this.constEval(ms, e.operand);
        return v === null ? null : -v;
      }
      case 'binary': {
        const l = this.constEval(ms, e.l);
        const r = this.constEval(ms, e.r);
        if (l === null || r === null) return null;
        switch (e.op) {
          case '+': return l + r;
          case '-': return l - r;
          case '*': return l * r;
          case '/': return r === 0n ? null : l / r;
          case '<<': return l << r;
          case '>>': return l >> r;
          case '&': return l & r;
          case '|': return l | r;
          case '^': return l ^ r;
          default: return null;
        }
      }
      case 'ident': {
        const res = this.resolveUnqualified(ms, e.name);
        if (!res || res.ambiguous === true || res.sym.kind !== 'const') return null;
        const cd = res.sym.decl as unknown as ConstDecl;
        const v = this.constEval(res.mod, cd.init);
        return v;
      }
      case 'member': {
        if (e.obj.kind !== 'ident') return null;
        const res = this.resolveUnqualified(ms, e.obj.name);
        if (!res || res.ambiguous === true || res.sym.kind !== 'enum') return null;
        const ed = res.sym.decl as unknown as EnumDecl;
        let next = 0n;
        for (const m of ed.members) {
          const val = m.value ?? next;
          next = val + 1n;
          if (m.name === e.field) return val;
        }
        return null;
      }
      default:
        return null;
    }
  }

  // ---------------------------------------------------------------------------
  // Systems + scheduling
  // ---------------------------------------------------------------------------

  private checkSystem(ms: ModuleSym, d: SystemDecl): void {
    if (d.every <= 0n) {
      this.sink.error('FORM-E-506', d.span, `system '${d.name}' has every ${d.every} (rate must be >= 1)`);
    }
    if (d.staggerPool !== null) {
      const rate = d.staggerRate ?? d.every;
      if (d.staggerRate !== null && d.staggerRate !== d.every) {
        this.sink.error('FORM-E-507', d.staggerSpan!,
          `stagger rate ${d.staggerRate} does not equal the system's every ${d.every} (FORM-E-507)`);
      }
      const r = this.resolveUnqualified(ms, d.staggerPool);
      if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
        this.sink.error('FORM-E-504', d.staggerSpan!,
          `stagger requires exactly one iteration pool — '${d.staggerPool}' is not a declared pool (FORM-E-504)`);
      }
      void rate;
      this.checkStaggerShape(ms, d);
    }
    const reads = new Set<string>();
    const writes = new Set<string>();
    for (const a of d.reads) this.validateAccess(ms, a, reads, false);
    for (const a of d.writes) this.validateAccess(ms, a, writes, true);
    // NOTE: a component in both reads and writes of ONE system is legal
    // (read-at-entry, write-at-exit = the F(state_t) law). FORM-E-407 fires
    // only when that overlap makes the phase order unsatisfiable — reported
    // from the schedule cycle walk below.
    const ctx = this.ctxFor(ms, 'sim');
    ctx.reads = reads;
    ctx.writes = writes;
    ctx.laterLets = collectLetNames(d.body);
    this.checkStmts(ctx, d.body);
  }

  /**
   * Stagger is a per-entity partition, not a system-rate guard.  Admission must
   * identify exactly one iteration/apply site so lowering cannot accidentally
   * stagger unrelated state effects.
   */
  private checkStaggerShape(ms: ModuleSym, d: SystemDecl): void {
    const pool = d.staggerPool!;
    const loops: Extract<Stmt, { kind: 'for' }>[] = [];
    const flowCalls: Extract<Stmt, { kind: 'call_stmt' }>[] = [];
    const walk = (stmts: Stmt[]): void => {
      for (const stmt of stmts) {
        if (stmt.kind === 'for') {
          loops.push(stmt);
          walk(stmt.body);
        } else if (stmt.kind === 'if') {
          walk(stmt.then);
          if (Array.isArray(stmt.else)) walk(stmt.else);
          else if (stmt.else) walk([stmt.else]);
        } else if (stmt.kind === 'call_stmt' && this.isFlowCallFor(ms, stmt.call, pool)) {
          flowCalls.push(stmt);
        }
      }
    };
    walk(d.body);

    const topLoops = d.body.filter((stmt): stmt is Extract<Stmt, { kind: 'for' }> =>
      stmt.kind === 'for' && this.isExactStaggerLoop(ms, stmt, pool));
    const topFlows = d.body.filter((stmt): stmt is Extract<Stmt, { kind: 'call_stmt' }> =>
      stmt.kind === 'call_stmt' && this.isFlowCallFor(ms, stmt.call, pool));
    const candidates: Stmt[] = [...topLoops, ...topFlows];
    const reasons: string[] = [];
    if (candidates.length !== 1) {
      reasons.push(`expected one top-level iteration/application over '${pool}', found ${candidates.length}`);
    }

    const selected = candidates.length === 1 ? candidates[0]! : null;
    if (selected?.kind === 'for') {
      if (loops.length !== 1) reasons.push('a staggered explicit system may contain only its selected pool loop');
      if (flowCalls.length !== 0) reasons.push('a staggered explicit loop may not also apply a flow program');
    } else if (selected?.kind === 'call_stmt') {
      if (loops.length !== 0) reasons.push('an implicit staggered flow application may not contain another loop');
      if (flowCalls.length !== 1) reasons.push('an implicit staggered system must contain exactly one flow application');
    }

    if (d.writes.some((access) => access.parts[0] !== pool)) {
      reasons.push(`writes outside stagger pool '${pool}' are not admitted`);
    }

    const checkEffects = (stmts: Stmt[], insideSelected: boolean): void => {
      for (const stmt of stmts) {
        if (stmt.kind === 'for') {
          checkEffects(stmt.body, stmt === selected);
        } else if (stmt.kind === 'if') {
          checkEffects(stmt.then, insideSelected);
          if (Array.isArray(stmt.else)) checkEffects(stmt.else, insideSelected);
          else if (stmt.else) checkEffects([stmt.else], insideSelected);
        } else if (stmt.kind === 'assign') {
          if (selected?.kind !== 'for' || !insideSelected || !this.isSelectedStaggerTarget(stmt.target, selected, pool)) {
            reasons.push('every state write must target the selected entity inside the stagger loop');
          }
        } else if (stmt.kind === 'spawn' || stmt.kind === 'kill' || stmt.kind === 'apply') {
          reasons.push(`${stmt.kind} is not admitted in a staggered system`);
        } else if (stmt.kind === 'call_stmt' && this.isAnyFlowCall(ms, stmt.call) && stmt !== selected) {
          reasons.push('only the selected flow application may mutate a staggered pool');
        }
      }
    };
    checkEffects(d.body, false);

    if (reasons.length !== 0) {
      this.sink.error('FORM-E-504', d.staggerSpan ?? d.span,
        `stagger over '${pool}' is not a single isolated entity iteration: ${[...new Set(reasons)].join('; ')} (FORM-E-504)`);
    }
  }

  private isExactStaggerLoop(ms: ModuleSym, stmt: Extract<Stmt, { kind: 'for' }>, pool: string): boolean {
    if (stmt.range.kind === 'pool') return stmt.range.pool === pool;
    const lo = this.constEval(ms, stmt.range.lo);
    const hi = stmt.range.hi;
    return lo === 0n
      && hi.kind === 'member'
      && hi.field === 'count'
      && hi.obj.kind === 'ident'
      && hi.obj.name === pool;
  }

  private isFlowCallFor(ms: ModuleSym, expr: Expr, pool: string): boolean {
    return this.isAnyFlowCall(ms, expr)
      && expr.kind === 'call'
      && expr.args[0]?.kind === 'ident'
      && expr.args[0].name === pool;
  }

  private isAnyFlowCall(ms: ModuleSym, expr: Expr): boolean {
    if (expr.kind !== 'call' || expr.callee.kind !== 'ident') return false;
    const resolved = this.resolveUnqualified(ms, expr.callee.name);
    return !!resolved && resolved.ambiguous !== true && resolved.sym.kind === 'field'
      && (resolved.sym.decl as unknown as FieldDecl).profile === 'flow';
  }

  private isSelectedStaggerTarget(
    target: Expr,
    loop: Extract<Stmt, { kind: 'for' }>,
    pool: string,
  ): boolean {
    const root = this.rootIdentOf(target)?.name;
    if (loop.range.kind === 'pool') return root === loop.varName;
    if (root !== pool) return false;
    let current: Expr = target;
    while (current.kind === 'member' || current.kind === 'index') {
      if (current.kind === 'index'
          && current.index.kind === 'ident'
          && current.index.name === loop.varName) return true;
      current = current.obj;
    }
    return false;
  }

  /** An access is pool[.field] | global | terrain | input. */
  private validateAccess(ms: ModuleSym, a: { parts: string[]; span: SourceSpan }, into: Set<string>, isWrite: boolean): void {
    const [first, second] = a.parts;
    if (first === 'terrain' || first === 'input') {
      into.add(first);
      return;
    }
    const r = this.resolveUnqualified(ms, first!);
    if (!r || r.ambiguous === true) {
      this.sink.error('FORM-E-203', a.span, `unknown access '${first}'`);
      return;
    }
    if (r.sym.kind === 'global') {
      if (second) this.sink.error('FORM-E-203', a.span, `global '${first}' has no component '${second}'`);
      into.add(first!);
      return;
    }
    if (r.sym.kind === 'pool') {
      if (!second) { into.add(first!); return; }
      const pd = r.sym.decl as unknown as PoolDecl;
      const sd = r.mod.table.get(pd.structName);
      if (!sd || sd.kind !== 'struct' || !(sd.decl as unknown as StructDecl).fields.some((f) => f.name === second)) {
        this.sink.error('FORM-E-306', a.span,
          `pool '${first}' struct '${pd.structName}' has no field '${second}'`);
      }
      into.add(`${first}.${second}`);
      return;
    }
    this.sink.error('FORM-E-203', a.span,
      `'${first}' is not a state component (pool, global, terrain or input)`);
    void isWrite;
  }

  /** The D6 analysis: phases by topological level, one writer per phase. */
  private computeSchedule(): Schedule | null {
    const systems: { decl: SystemDecl; mod: ModuleSym }[] = [];
    for (const ms of this.mods) {
      for (const d of ms.ast.decls) if (d.kind === 'System') systems.push({ decl: d, mod: ms });
    }
    if (systems.length === 0) return { phases: [] };

    // component -> writers/readers. A whole-pool access expands to every
    // struct-field component plus the membership component `<pool>#members`
    // (spawn/kill), so whole-pool and field-level accesses conflict correctly.
    const accessComponents = (a: { parts: string[] }, mod: ModuleSym): string[] => {
      const [first, second] = a.parts;
      if (first === 'terrain' || first === 'input') return [first];
      const r = this.resolveUnqualified(mod, first!);
      if (r && r.ambiguous !== true && r.sym.kind === 'pool') {
        const pd = r.sym.decl as unknown as PoolDecl;
        const sd = r.mod.table.get(pd.structName);
        if (!second) {
          const comps = [`${first}#members`];
          if (sd && sd.kind === 'struct') {
            for (const f of (sd.decl as unknown as StructDecl).fields) comps.push(`${first}.${f.name}`);
          }
          return comps;
        }
        return [`${first}.${second}`];
      }
      return [first!];
    };
    const writers = new Map<string, { decl: SystemDecl; mod: ModuleSym }[]>();
    const readers = new Map<string, { decl: SystemDecl; mod: ModuleSym }[]>();
    for (const s of systems) {
      for (const a of s.decl.writes) {
        for (const comp of accessComponents(a, s.mod)) {
          const list = writers.get(comp) ?? [];
          list.push(s);
          writers.set(comp, list);
        }
      }
      for (const a of s.decl.reads) {
        for (const comp of accessComponents(a, s.mod)) {
          const list = readers.get(comp) ?? [];
          list.push(s);
          readers.set(comp, list);
        }
      }
    }

    // levels: reader of C strictly after every OTHER writer of C; terrain
    // reads see tick start (deterministic-scheduling §2) so 'terrain' reads
    // impose no ordering.
    const level = new Map<SystemDecl, number>();
    for (const s of systems) level.set(s.decl, 0);
    const n = systems.length;
    for (let iter = 0; iter <= n; iter++) {
      let changed = false;
      for (const s of systems) {
        let lvl = level.get(s.decl)!;
        for (const a of s.decl.reads) {
          if (a.parts[0] === 'terrain') continue;
          for (const comp of accessComponents(a, s.mod)) {
            for (const w of writers.get(comp) ?? []) {
              if (w.decl === s.decl) continue;
              const need = level.get(w.decl)! + 1;
              if (need > lvl) { lvl = need; changed = true; }
            }
          }
        }
        level.set(s.decl, lvl);
      }
      if (!changed) break;
      if (iter === n) {
        // unsatisfiable order. FORM-E-407 when a system in the cycle declares
        // the same component in both reads and writes (the read is only
        // satisfiable pre-write — separation the language cannot express);
        // otherwise a plain read-write cycle (FORM-E-505).
        const cyc = this.findCycle(systems, writers, accessComponents);
        const wSets = new Map<SystemDecl, Set<string>>();
        for (const s of systems) {
          const set = new Set<string>();
          for (const a of s.decl.writes) for (const comp of accessComponents(a, s.mod)) set.add(comp);
          wSets.set(s.decl, set);
        }
        let overlap: { decl: SystemDecl; comp: string } | null = null;
        for (const s of systems) {
          if (!wSets.has(s.decl)) continue;
          for (const a of s.decl.reads) {
            for (const comp of accessComponents(a, s.mod)) {
              if (wSets.get(s.decl)!.has(comp)) { overlap = { decl: s.decl, comp }; break; }
            }
            if (overlap) break;
          }
          if (overlap) break;
        }
        if (overlap) {
          this.sink.error('FORM-E-407', overlap.decl.span,
            `system '${overlap.decl.name}' reads and writes '${overlap.comp}' while the phase order needs separation (the read is only satisfiable pre-write; split the system or drop the reads entry — FORM-E-407, cycle ${cyc.names})`);
        } else {
          this.sink.error('FORM-E-505', cyc.span,
            `cyclic read-write dependency between systems (${cyc.names}) — no topological order exists (FORM-E-505); split a system or weaken a read`);
        }
        return null;
      }
    }

    // group by level; one writer per component per phase (E-500, both spans)
    const maxLevel = Math.max(...[...level.values()]);
    const phases: Schedule['phases'] = [];
    for (let i = 0; i <= maxLevel; i++) {
      const bucket = systems.filter((s) => level.get(s.decl) === i);
      const phaseWriters = new Map<string, SystemDecl>();
      for (const s of bucket) {
        for (const a of s.decl.writes) {
          for (const comp of accessComponents(a, s.mod)) {
            const prev = phaseWriters.get(comp);
            if (prev && prev !== s.decl) {
              this.sink.error('FORM-E-500', a.span,
                `two systems write '${comp}' in one phase: '${prev.name}' (${prev.span.file}:${prev.span.start}) and '${s.decl.name}' (${a.span.file}:${a.span.start}) — both spans cited; one writer per component per phase (FORM-E-500, D6)`);
            } else {
              phaseWriters.set(comp, s.decl);
            }
          }
        }
      }
      phases.push({
        index: i,
        systems: bucket.map((s) => ({
          name: s.decl.name, module: s.mod.ast.name, every: s.decl.every, span: s.decl.span,
        })),
      });
    }
    return { phases };
  }

  private findCycle(
    systems: { decl: SystemDecl; mod: ModuleSym }[],
    writers: Map<string, { decl: SystemDecl; mod: ModuleSym }[]>,
    accessComponents: (a: { parts: string[] }, mod: ModuleSym) => string[],
  ): { span: SourceSpan; names: string } {
    // DFS for a back edge S ->read(C)-> W ->...-> S
    const color = new Map<SystemDecl, 0 | 1 | 2>();
    const stack: SystemDecl[] = [];
    let found: SystemDecl | null = null;
    const dfs = (s: { decl: SystemDecl; mod: ModuleSym }): void => {
      const c = color.get(s.decl) ?? 0;
      if (c === 1) { found = s.decl; return; }
      if (c === 2) return;
      color.set(s.decl, 1);
      stack.push(s.decl);
      for (const a of s.decl.reads) {
        if (a.parts[0] === 'terrain') continue;
        for (const comp of accessComponents(a, s.mod)) {
          for (const w of writers.get(comp) ?? []) {
            if (w.decl === s.decl) continue;
            dfs(w);
            if (found) return;
          }
        }
      }
      stack.pop();
      color.set(s.decl, 2);
    };
    for (const s of systems) { dfs(s); if (found) break; }
    if (!found) found = systems[0]!.decl;
    const idx = stack.indexOf(found);
    const names = (idx >= 0 ? stack.slice(idx) : [found]).map((s) => s.name).join(' -> ');
    return { span: found.span, names };
  }

  // ---------------------------------------------------------------------------
  // Presentations, scenarios, sounds
  // ---------------------------------------------------------------------------

  private checkPresentation(ms: ModuleSym, d: PresentationDecl): void {
    const ctx = this.ctxFor(ms, 'present');
    const views: { id: bigint; pct: bigint; span: SourceSpan }[] = [];
    let shared = 0n;
    for (const item of d.items) {
      if (item.kind === 'view') {
        views.push({ id: item.id, pct: item.budgetPct, span: item.span });
        if (item.camera === null) {
          this.sink.error('FORM-E-607', item.span,
            `view ${item.id} has no camera binding ('view N from <world3> budget P%')`);
        } else {
          const ct = this.checkExpr(ctx, item.camera, T.world3);
          if (!tAgree(ct, T.world3) && !tAgree(ct, T.unknown)) {
            this.sink.error('FORM-E-464', item.camera.span,
              `camera binding must be a world3 transform source, got ${typeName(ct)} (FORM-E-464)`);
          }
        }
      } else if (item.kind === 'shared_budget') {
        shared = item.pct;
      } else if (item.kind === 'emit') {
        this.checkEmit(ctx, item, ms, d);
      } else if (item.kind === 'assign') {
        this.sink.error('FORM-E-405', item.span,
          'presentation blocks never mutate — assignment is refused (present-purity law, FORM-E-405)');
      } else if (item.kind === 'spawn') {
        this.sink.error('FORM-E-405', item.span, 'presentation blocks never mutate: spawn refused (FORM-E-405)');
      } else if (item.kind === 'kill') {
        this.sink.error('FORM-E-405', item.span, 'presentation blocks never mutate: kill refused (FORM-E-405)');
      } else if (item.kind === 'apply') {
        this.sink.error('FORM-E-405', item.span, 'presentation blocks never mutate: apply refused (FORM-E-405)');
      } else if (item.kind === 'call_stmt') {
        // pure fn calls are fine (pure readers); mutating targets diagnose themselves
        this.checkExpr(ctx, item.call, null);
      } else if (item.kind === 'let' || item.kind === 'if' || item.kind === 'for' || item.kind === 'return') {
        this.sink.error('FORM-E-108', item.span,
          `statement '${item.kind}' is not admitted in a presentation block (view/shared/emit only, FORM-E-108)`);
      }
    }
    if (views.length > 2) {
      this.sink.error('FORM-E-604', d.span,
        `${views.length} views declared — at most two in L1 (the Duo law, FORM-E-604)`);
    }
    const seenIds = new Set<bigint>();
    for (const v of views) {
      if (v.id !== 0n && v.id !== 1n) {
        this.sink.error('FORM-E-606', v.span, `view id ${v.id} is not 0/1 (FORM-E-606)`);
      }
      if (seenIds.has(v.id)) {
        this.sink.error('FORM-E-606', v.span, `view id ${v.id} declared twice (FORM-E-606)`);
      }
      seenIds.add(v.id);
    }
    const sum = views.reduce((a, v) => a + v.pct, 0n) + shared;
    if (sum > 100n) {
      this.sink.error('FORM-E-605', d.span,
        `view budgets (${views.map((v) => v.pct).join('% + ')}%) plus shared (${shared}%) sum to ${sum}% > 100% (FORM-E-605)`);
    }
  }

  private static readonly EMIT_ARGS: Record<string, Record<string, string>> = {
    draw_form: { form: 'u32const', transform: 'world3', view_mask: 'u32', weight: 'unit8' },
    draw_population: { pool: 'pool', view_mask: 'u32', weight: 'unit8' },
    draw_procedural: { patch: 'u32const', transform: 'world3', screen_error: 'fx16' },
    surface_stamp: { brush: 'u32const', at: 'world3', radius: 'fx16', ring_width: 'fx16', tag: 'u32', strength: 'unit8' },
    audio: { sound: 'sound', at: 'world3' },
  };

  private checkEmit(ctx: Ctx, e: EmitStmt, ms: ModuleSym, pres: PresentationDecl): void {
    const spec = Checker.EMIT_ARGS[e.emitKind];
    if (!spec) return; // parser already raised E-603/E-717
    const seen = new Set<string>();
    for (const arg of e.args) {
      const kind = spec[arg.name];
      if (!kind) {
        this.sink.error('FORM-E-602', arg.span,
          `unknown emit argument '${arg.name}' for ${e.emitKind} (expects ${Object.keys(spec).join(', ')})`);
        this.checkExpr(ctx, arg.value, null);
        continue;
      }
      if (seen.has(arg.name)) {
        this.sink.error('FORM-E-602', arg.span, `duplicate emit argument '${arg.name}'`);
      }
      seen.add(arg.name);
      switch (kind) {
        case 'u32const': {
          this.checkExpr(ctx, arg.value, T.u32);
          const v = this.constEval(ms, arg.value);
          if (v === null) {
            this.sink.error('FORM-E-610', arg.value.span,
              `resource page-id argument '${arg.name}' must be a u32 const (a page-id constant or integer; FORM-E-610)`);
          } else if (v < 0n || v > 0xffffffffn) {
            this.sink.error('FORM-E-610', arg.value.span, `page-id ${v} out of u32 range (FORM-E-610)`);
          }
          break;
        }
        case 'pool': {
          if (arg.value.kind !== 'ident') {
            this.sink.error('FORM-E-608', arg.value.span, `'${arg.name}' must name a declared pool (FORM-E-608)`);
            break;
          }
          const r = this.resolveUnqualified(ms, arg.value.name);
          if (!r || r.ambiguous === true || r.sym.kind !== 'pool') {
            this.expressionTypes.set(arg.value, T.unknown);
            this.sink.error('FORM-E-608', arg.value.span,
              `draw_population names '${arg.value.name}' which is not a pool (FORM-E-608)`);
          } else {
            const pd = r.sym.decl as unknown as PoolDecl;
            this.expressionTypes.set(arg.value, { t: 'pool', name: arg.value.name, struct: pd.structName });
          }
          break;
        }
        case 'sound': {
          if (arg.value.kind !== 'ident') {
            this.sink.error('FORM-E-609', arg.value.span, `'${arg.name}' must name a declared sound (FORM-E-609)`);
            break;
          }
          const r = this.resolveUnqualified(ms, arg.value.name);
          if (!r || r.ambiguous === true || r.sym.kind !== 'sound') {
            this.expressionTypes.set(arg.value, T.unknown);
            this.sink.error('FORM-E-609', arg.value.span,
              `audio names '${arg.value.name}' which is not a declared sound (FORM-E-609)`);
          } else {
            this.expressionTypes.set(arg.value, T.sound);
            if (this.findLaterSound(ms, arg.value.name, pres)) {
              this.sink.error('FORM-E-408', arg.value.span,
                `sound '${arg.value.name}' is referenced before its declaration — sounds are declaration-ordered (FORM-E-408)`);
            }
          }
          break;
        }
        default: {
          const want = SCALARS[kind] ?? T.unknown;
          this.checkExpr(ctx, arg.value, want);
        }
      }
    }
    for (const name of Object.keys(spec)) {
      if (!seen.has(name)) {
        this.sink.error('FORM-E-601', e.span,
          `${e.emitKind} is missing its '${name}' argument (FORM-E-601)`);
      }
    }
  }

  private findLaterSound(ms: ModuleSym, name: string, pres: PresentationDecl): boolean {
    const sym = ms.table.get(name);
    if (!sym || sym.kind !== 'sound') return false;
    return sym.order > (ms.table.get(pres.name)?.order ?? 0);
  }

  private checkScenario(ms: ModuleSym, d: ScenarioDecl): void {
    const ctx = this.ctxFor(ms, 'scenario');
    let seeds = 0;
    let lastTick: bigint | null = null;
    let sawAt = false;
    for (const item of d.items) {
      switch (item.kind) {
        case 'seed':
          seeds++;
          if (seeds > 1) {
            this.sink.error('FORM-E-900', item.span, `scenario '${d.name}' repeats its seed (FORM-E-900)`);
          }
          break;
        case 'load': {
          if (!this.byName.has(item.target)) {
            this.sink.error('FORM-E-901', item.span,
              `load target '${item.target}' is not a module known to the cartridge (FORM-E-901)`);
          }
          break;
        }
        case 'spawn_player':
          if (item.index < 0n || item.index > 3n) {
            this.sink.error('FORM-E-902', item.span,
              `spawn player ${item.index} — index must be 0..3 (FORM-E-902)`);
          }
          break;
        case 'at': {
          if (lastTick !== null && item.tick <= lastTick) {
            this.sink.error('FORM-E-903', item.span,
              `at ${item.tick} ticks does not ascend (previous ${lastTick}) — scenario scripts are ascending (FORM-E-903)`);
          }
          lastTick = item.tick;
          sawAt = true;
          if (item.action.kind === 'call' && item.action.callee.kind === 'ident') {
            const r = this.resolveUnqualified(ms, item.action.callee.name);
            if (!r || r.ambiguous === true || r.sym.kind !== 'system') {
              this.expressionTypes.set(item.action, T.unknown);
              this.sink.error('FORM-E-904', item.action.span,
                `scenario action '${item.action.callee.name}' is not a declared system entry (FORM-E-904)`);
            } else {
              this.expressionTypes.set(item.action, T.void);
              if (item.action.args.length !== 0) {
                this.sink.error('FORM-E-304', item.action.span, 'scenario system entries take no arguments');
              }
              for (const arg of item.action.args) this.checkExpr(ctx, arg, null);
            }
          } else {
            this.sink.error('FORM-E-904', item.action.span,
              'scenario actions must name a system entry (FORM-E-904)');
          }
          break;
        }
        case 'assert': {
          const a = { ...ctx, inAssert: true } as Ctx;
          this.checkExpr(a, item.expr, T.bool);
          if (item.tolerance !== null) {
            const tctx = { ...ctx, inTolerance: true } as Ctx;
            const tt = this.checkExpr(tctx, item.tolerance, T.fx16);
            // tolerance must be an exactly-representable fx16 (E-907)
            const te = item.tolerance;
            if (te.kind === 'literal' && te.lit === 'frac' && tt.t === 'fx16') {
              const f = te.frac!;
              const num = BigInt(f.intDigits + (f.fracDigits || ''));
              const den = 10n ** BigInt(f.fracDigits.length || 0);
              void num; void den; // exactness handled by the literal law (E-907 in this position)
            }
          }
          break;
        }
        case 'capture': {
          if (sawAt && lastTick !== null && item.frame <= lastTick) {
            this.sink.error('FORM-E-905', item.span,
              `capture frame ${item.frame} is not after the last scheduled action tick ${lastTick} (FORM-E-905)`);
          }
          break;
        }
        case 'assert_budget': {
          // L1 budget sets are presentation declarations (L3 registry per E-720)
          const r = this.resolveUnqualified(ms, item.budgetSet);
          if (!r || r.ambiguous === true || r.sym.kind !== 'presentation') {
            this.sink.error('FORM-E-720', item.span,
              `assert_budget names '${item.budgetSet}' which is not a declared budget set (a presentation; L3 registry, FORM-E-720)`);
          }
          break;
        }
      }
    }
    if (seeds === 0) {
      this.sink.error('FORM-E-900', d.span,
        `scenario '${d.name}' has no seed — deterministic scripts are seeded (FORM-E-900)`);
    }
  }

  private checkSound(ms: ModuleSym, d: SoundDecl): void {
    const ctx = this.ctxFor(ms, 'fn');
    for (const p of d.params) {
      if (p.kind === 'gain') this.checkExpr(ctx, p.value, T.unit8);
      else if (p.kind === 'pitch') this.checkExpr(ctx, p.value, T.fx16);
      else this.checkExpr(ctx, p.value, T.i32);
    }
  }

  // ---------------------------------------------------------------------------
  // Recursion (E-709)
  // ---------------------------------------------------------------------------

  private recordFnEdge(from: string, to: string, span: SourceSpan): void {
    const list = this.fnEdges.get(from) ?? [];
    list.push({ to, span });
    this.fnEdges.set(from, list);
  }

  private checkRecursion(): void {
    const color = new Map<string, 0 | 1 | 2>();
    const stack: string[] = [];
    let reported = false;
    const dfs = (name: string, span: SourceSpan): void => {
      const c = color.get(name) ?? 0;
      if (c === 1) {
        if (!reported) {
          reported = true;
          const idx = stack.indexOf(name);
          const cycle = (idx >= 0 ? stack.slice(idx) : [name]).concat(name).join(' -> ');
          this.sink.error('FORM-E-709', span,
            `recursion refused (FORM-E-709; FORM §6 bounded-execution law): ${cycle}`);
        }
        return;
      }
      if (c === 2) return;
      color.set(name, 1);
      stack.push(name);
      for (const edge of this.fnEdges.get(name) ?? []) dfs(edge.to, edge.span);
      stack.pop();
      color.set(name, 2);
    };
    for (const name of [...this.fnEdges.keys()].sort()) {
      const first = (this.fnEdges.get(name) ?? [])[0];
      dfs(name, first ? first.span : { file: '<recursion>', start: 0, end: 0 });
    }
  }

  // ---------------------------------------------------------------------------
  // fn calls + intrinsics
  // ---------------------------------------------------------------------------

  private checkCall(ctx: Ctx, e: Extract<Expr, { kind: 'call' }>, expected: Type | null): Type {
    // dotted intrinsics: input.*, random.*, terrain.*
    if (e.callee.kind === 'member' && e.callee.obj.kind === 'ident') {
      const base = e.callee.obj.name;
      const fn = e.callee.field;
      if (base === 'input' || base === 'random' || base === 'terrain') {
        return this.checkIntrinsicCall(ctx, e, `${base}.${fn}`, expected);
      }
    }
    // plain fn call
    if (e.callee.kind === 'ident') {
      const name = e.callee.name;
      // bare-name built-ins (§4.6) — the builtin table wins over user names
      if (BARE_INTRINSICS.has(name)) {
        return this.checkIntrinsicCall(ctx, e, name, expected);
      }
      const r = this.resolveUnqualified(ctx.mod, name);
      if (r && r.ambiguous !== true) {
        if (r.sym.kind === 'fn') {
          if (ctx.domain === 'field') {
            this.sink.error('FORM-E-652', e.span,
              `call to fn '${name}' inside a field body — not even pure fns (branchless law, FORM-E-652)`);
            return T.unknown;
          }
          const fd = r.sym.decl as unknown as FnDecl;
          this.recordFnEdge(currentFn(ctx), `${r.mod.ast.name}::${name}`, e.span);
          if (fd.params.length !== e.args.length) {
            this.sink.error('FORM-E-304', e.span,
              `wrong argument count in call to '${name}' (${e.args.length} for ${fd.params.length})`);
          }
          // arguments evaluate in the CALLER's scope (params are not in scope)
          for (let i = 0; i < fd.params.length && i < e.args.length; i++) {
            const want = this.resolveType(r.mod, fd.params[i]!.type);
            this.checkExpr(ctx, e.args[i]!, want);
          }
          for (let i = fd.params.length; i < e.args.length; i++) this.checkExpr(ctx, e.args[i]!, null);
          return this.resolveType(r.mod, fd.ret);
        }
        if (r.sym.kind === 'field') {
          const fd = r.sym.decl as unknown as FieldDecl;
          if (ctx.domain === 'field') {
            this.sink.error('FORM-E-652', e.span,
              `call inside a field body — not even pure fns or field programs (FORM-E-652)`);
            return T.unknown;
          }
          if (fd.profile === 'flow' && ctx.domain === 'sim') {
            // per-element flow application: prog(pool, params)
            if (e.args.length < 1 || e.args[0]!.kind !== 'ident') {
              this.sink.error('FORM-E-667', e.span,
                `flow program '${name}' is applied per element: first argument is its mapped pool (FORM-E-667)`);
            } else {
              const poolName = e.args[0]!.name;
              const pool = this.resolveUnqualified(ctx.mod, poolName);
              const pd = pool && pool.ambiguous !== true && pool.sym.kind === 'pool'
                ? pool.sym.decl as unknown as PoolDecl : null;
              this.expressionTypes.set(e.args[0]!, pd
                ? { t: 'pool', name: poolName, struct: pd.structName }
                : T.unknown);
              this.checkFlowPoolMapping(ctx, poolName, e.args[0]!.span);
              this.writeComponent(ctx, poolName, e.args[0]!.span);
              this.readComponent(ctx, poolName, e.args[0]!.span);
            }
            const paramsType = fd.paramsStruct ? { t: 'struct', name: fd.paramsStruct } as Type : null;
            for (let i = 1; i < e.args.length; i++) this.checkExpr(ctx, e.args[i]!, i === 1 ? paramsType : null);
            return T.void;
          }
          this.sink.error('FORM-E-462', e.span,
            `field program '${name}' is applied with 'apply terrain_field ...' / per-element for flow, not called (FORM-E-462)`);
          return T.void;
        }
        if (r.sym.kind === 'system') {
          this.sink.error('FORM-E-904', e.span,
            `system '${name}' runs on the compile-time schedule — it is not callable (scenario actions name systems)`);
          return T.void;
        }
      }
      // unknown callee
      const code = ctx.inAssert ? 'FORM-E-906' : 'FORM-E-203';
      this.sink.error(code, e.span, `call to unknown name '${name}'`);
      for (const a of e.args) this.checkExpr(ctx, a, null);
      return T.unknown;
    }
    // non-ident callee
    this.checkExpr(ctx, e.callee, null);
    for (const a of e.args) this.checkExpr(ctx, a, null);
    this.sink.error('FORM-E-110', e.span, 'call target is not a function name');
    return T.unknown;
  }

  private checkIntrinsicCall(
    ctx: Ctx,
    e: Extract<Expr, { kind: 'call' }>,
    name: string,
    expected: Type | null,
  ): Type {
    const args = e.args;
    const argT = (want: Type | null = null): Type[] => args.map((a) => this.checkExpr(ctx, a, want));
    const sameNumericArgs = (): Type[] => {
      const contextual = expected && NUMERIC.has(expected.t) ? expected : null;
      const types = argT(contextual);
      const adopted = contextual ?? types.find((type, index) => !isBareInt(args[index]!) && adoptableLiteralType(type));
      if (adopted) {
        for (let index = 0; index < args.length; index++) {
          if (isBareInt(args[index]!)) types[index] = this.checkExpr(ctx, args[index]!, adopted);
        }
      }
      return types;
    };
    const demand = (allowed: Domain[], span: SourceSpan): boolean => {
      if (ctx.domain === 'field' && !allowed.includes('field')) {
        if (name === 'random.stream') {
          this.sink.error('FORM-E-404', span, 'random streams are refused in the field dialect (FORM-E-404)');
        } else if (name.startsWith('random') || name.startsWith('input')) {
          this.sink.error('FORM-E-657', span, `${name} inside a field body (FORM-E-657)`);
        } else {
          this.sink.error('FORM-E-665', span,
            `intrinsic ${name} is not admitted in the field dialect (§6.3 surface)`);
        }
        return false;
      }
      if (ctx.domain === 'field') return true;
      if (allowed.includes(ctx.domain as Domain)) return true;
      if (name.startsWith('input')) {
        this.sink.error('FORM-E-403', span,
          `input is readable only in sim systems and scenarios (FORM-E-403; ${ctx.domain} here)`);
      } else {
        this.sink.error('FORM-E-402', span,
          `${name} is not admitted in the ${ctx.domain} domain`);
      }
      return false;
    };
    const sameNumeric = (ts: Type[], span: SourceSpan): Type => {
      const first = ts[0] ?? T.unknown;
      if (!NUMERIC.has(first.t) && first.t !== 'unknown') {
        this.sink.error('FORM-E-300', span, `${name} expects numeric arguments, got ${typeName(first)}`);
        return T.unknown;
      }
      for (const t of ts.slice(1)) {
        if (!tAgree(t, first) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-300', span,
            `${name} arguments must share one type (${typeName(first)} vs ${typeName(t)})`);
        }
      }
      return first;
    };

    switch (name) {
      case 'input.player': {
        demand(['sim', 'scenario'], e.span);
        const argument = args[0] ?? none(e.span);
        const argType = this.checkExpr(ctx, argument, T.u32);
        const n = this.constEval(ctx.mod, argument);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, 'input.player(n) takes one u32 argument');
        if (!tAgree(argType, T.u32) && !tAgree(argType, T.i32) && !tAgree(argType, T.unknown)) {
          this.sink.error('FORM-E-300', argument.span, `input.player index must be u32, got ${typeName(argType)}`);
        }
        if (n !== null && (n < 0n || n > 3n)) {
          this.sink.error('FORM-E-300', e.span, `input.player(${n}) — pad index must be 0..3`);
        }
        this.readComponent(ctx, 'input', e.span);
        return T.padframe;
      }
      case 'input.held': {
        demand(['sim', 'scenario'], e.span);
        const pad = this.checkExpr(ctx, args[0] ?? none(e.span), T.padframe);
        const button = this.checkExpr(ctx, args[1] ?? none(e.span), T.u32);
        if (args.length !== 2) this.sink.error('FORM-E-304', e.span, 'input.held(p, b) takes two arguments');
        if (!tAgree(pad, T.padframe) && !tAgree(pad, T.unknown)) {
          this.sink.error('FORM-E-300', (args[0] ?? e).span, `input.held expects a PadFrame, got ${typeName(pad)}`);
        }
        if (!tAgree(button, T.u32) && !tAgree(button, T.i32) && !tAgree(button, T.unknown)) {
          this.sink.error('FORM-E-300', (args[1] ?? e).span, `input.held button must be u32, got ${typeName(button)}`);
        }
        return T.bool;
      }
      case 'random.stream': {
        demand(['sim', 'present', 'scenario'], e.span);
        for (const a of args) {
          const t = this.checkExpr(ctx, a, T.u32);
          if (!tAgree(t, T.u32) && !tAgree(t, T.i32) && !tAgree(t, T.unknown)) {
            this.sink.error('FORM-E-300', a.span, `random.stream ids must be u32, got ${typeName(t)}`);
          }
        }
        if (args.length < 1) this.sink.error('FORM-E-304', e.span, 'random.stream takes a seed');
        return T.stream;
      }
      case 'random.u32': case 'random.i32': case 'random.unit8': case 'random.angle16': {
        demand(['sim', 'present', 'scenario'], e.span);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, `${name}(s) takes one stream`);
        const t = this.checkExpr(ctx, args[0] ?? none(e.span), T.stream);
        if (!tAgree(t, T.stream) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-334', e.span, `${name} expects a stream, got ${typeName(t)}`);
        }
        return { t: name.slice(7) } as Type;
      }
      case 'random.fx16': {
        demand(['sim', 'present', 'scenario'], e.span);
        if (args.length !== 3) this.sink.error('FORM-E-304', e.span, 'random.fx16(s, lo, hi) takes three arguments');
        const stream = this.checkExpr(ctx, args[0] ?? none(e.span), T.stream);
        const lo = this.checkExpr(ctx, args[1] ?? none(e.span), T.fx16);
        const hi = this.checkExpr(ctx, args[2] ?? none(e.span), T.fx16);
        if (!tAgree(stream, T.stream) && !tAgree(stream, T.unknown)) {
          this.sink.error('FORM-E-334', e.span, 'random.fx16 expects a stream first');
        }
        for (const [arg, type] of [[args[1], lo], [args[2], hi]] as const) {
          if (arg && !tAgree(type, T.fx16) && !tAgree(type, T.unknown)) {
            this.sink.error('FORM-E-300', arg.span, `random.fx16 bounds must be fx16, got ${typeName(type)}`);
          }
        }
        return T.fx16;
      }
      case 'terrain.height': {
        demand(['sim', 'present', 'scenario'], e.span);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, 'terrain.height(world2) takes one world2');
        const t = this.checkExpr(ctx, args[0] ?? none(e.span), T.world2);
        if (!tAgree(t, T.world2) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-300', e.span, `terrain.height expects world2, got ${typeName(t)}`);
        }
        this.readComponent(ctx, 'terrain', e.span);
        return T.fx16;
      }
      case 'abs': case 'min': case 'max': {
        demand(['sim', 'fn', 'present', 'scenario', 'field'], e.span);
        if (name !== 'abs' && args.length !== 2) this.sink.error('FORM-E-304', e.span, `${name}(a, b) takes two arguments`);
        if (name === 'abs' && args.length !== 1) this.sink.error('FORM-E-304', e.span, 'abs(x) takes one argument');
        return sameNumeric(sameNumericArgs(), e.span);
      }
      case 'clamp': {
        demand(['sim', 'fn', 'present', 'scenario', 'field'], e.span);
        if (args.length !== 3) this.sink.error('FORM-E-304', e.span, 'clamp(x, lo, hi) takes three arguments');
        return sameNumeric(sameNumericArgs(), e.span);
      }
      case 'sin': case 'cos': {
        demand(['sim', 'fn', 'present', 'scenario', 'field'], e.span);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, `${name}(a) takes one angle16`);
        const t = this.checkExpr(ctx, args[0] ?? none(e.span), T.angle16);
        if (!tAgree(t, T.angle16) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-300', e.span, `${name} expects angle16, got ${typeName(t)}`);
        }
        return T.fx16;
      }
      case 'atan2_approx': {
        demand(['sim', 'scenario', 'fn'], e.span);
        if (args.length !== 2) this.sink.error('FORM-E-304', e.span, 'atan2_approx(y, x) takes two fx16');
        for (const a of args) {
          const t = this.checkExpr(ctx, a, T.fx16);
          if (!tAgree(t, T.fx16) && !tAgree(t, T.unknown)) {
            this.sink.error('FORM-E-300', a.span, `atan2_approx expects fx16, got ${typeName(t)}`);
          }
        }
        return T.angle16;
      }
      case 'sqrt_approx': {
        demand(['sim', 'scenario', 'fn'], e.span);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, 'sqrt_approx(x) takes one fx16');
        const t = this.checkExpr(ctx, args[0] ?? none(e.span), T.fx16);
        if (!tAgree(t, T.fx16) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-300', e.span, `sqrt_approx expects fx16, got ${typeName(t)}`);
        }
        return T.fx16;
      }
      case 'to_fx16': case 'to_fx24': case 'to_unit8': case 'to_angle16': {
        demand(['sim', 'fn', 'present', 'scenario'], e.span);
        if (args.length !== 1) this.sink.error('FORM-E-304', e.span, `${name}(x) takes one argument`);
        const t = this.checkExpr(ctx, args[0] ?? none(e.span), null);
        const wants: Record<string, Type[]> = {
          to_fx16: [T.fx24, T.fx16, T.angle16, T.unit8],
          to_fx24: [T.fx16],
          to_unit8: [T.fx16],
          to_angle16: [T.fx16],
        };
        if (!wants[name]!.some((w) => tAgree(t, w)) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-334', e.span,
            `${name} argument must be ${wants[name]!.map(typeName).join(' or ')}, got ${typeName(t)} (FORM-E-334)`);
        }
        return { t: name.slice(3) } as Type;
      }
      case 'dot2': case 'dot3': case 'length': case 'normalize': {
        const arity = name === 'dot2' || name === 'dot3' ? 2 : 1;
        if (args.length !== arity) {
          this.sink.error('FORM-E-304', e.span, `${name} takes ${arity === 1 ? 'one argument' : 'two arguments'}`);
        }
        if (ctx.domain === 'field') {
          // dialect form (§6.3): fx16 builder lanes, one builder op
          for (const a of args) {
            const t = this.checkExpr(ctx, a, T.fx16);
            if (!tAgree(t, T.fx16) && !tAgree(t, T.unknown)) {
              this.sink.error('FORM-E-658', a.span,
                `field-dialect operand must be fx16, got ${typeName(t)} (FORM-E-658)`);
            }
          }
          return T.fx16;
        }
        demand(['sim', 'scenario', 'fn'], e.span);
        const ts = argT();
        if (!ts.every((t) => VECTORS.has(t.t) || tAgree(t, T.unknown))) {
          this.sink.error('FORM-E-300', e.span, `${name} expects vector arguments, got ${ts.map(typeName).join(', ')}`);
          return name === 'normalize' ? T.unknown : T.fx24;
        }
        if (ts.length > 1 && !ts.every((t) => tAgree(t, ts[0] ?? T.unknown))) {
          this.sink.error('FORM-E-330', e.span, `${name} on mixed vector types (${ts.map(typeName).join(', ')})`);
        }
        if (name === 'normalize') return ts[0] ?? T.unknown;
        if (name === 'length') return T.fx16;
        return T.fx24;
      }
      case 'mix': {
        demand(['sim', 'present', 'scenario'], e.span);
        if (args.length !== 3) this.sink.error('FORM-E-304', e.span, 'mix(a, b, t) takes three arguments');
        const contextual = expected && (NUMERIC.has(expected.t) || VECTORS.has(expected.t)) ? expected : null;
        let a = this.checkExpr(ctx, args[0] ?? none(e.span), contextual);
        let b = this.checkExpr(ctx, args[1] ?? none(e.span), contextual);
        if (isBareInt(args[0]) && adoptableLiteralType(b)) a = this.checkExpr(ctx, args[0]!, b);
        if (isBareInt(args[1]) && adoptableLiteralType(a)) b = this.checkExpr(ctx, args[1]!, a);
        const weight = this.checkExpr(ctx, args[2] ?? none(e.span), T.unit8);
        if (!tAgree(a, b) && !tAgree(a, T.unknown) && !tAgree(b, T.unknown)) {
          this.sink.error('FORM-E-300', e.span, `mix values must share one type (${typeName(a)} vs ${typeName(b)})`);
        }
        if (!NUMERIC.has(a.t) && !VECTORS.has(a.t) && !tAgree(a, T.unknown)) {
          this.sink.error('FORM-E-300', e.span, `mix does not admit ${typeName(a)}`);
        }
        if (!tAgree(weight, T.unit8) && !tAgree(weight, T.unknown)) {
          this.sink.error('FORM-E-300', (args[2] ?? e).span, `mix weight must be unit8, got ${typeName(weight)}`);
        }
        return tAgree(a, T.unknown) ? b : a;
      }
      default:
        break;
    }

    // field-dialect op table (§6.3) — only inside field bodies
    const FIELD_OPS = new Set(['dot2', 'dot3', 'length2', 'length3', 'dist', 'normalize2',
      'normalize3', 'rcp', 'curve', 'spline', 'dcurve', 'noise2', 'ridge', 'ring',
      'rot2', 'rot3', 'smoothstep']);
    if (FIELD_OPS.has(name)) {
      if (ctx.domain !== 'field') {
        this.sink.error('FORM-E-203', e.span,
          `unknown name '${name}' (field-dialect ops live inside @earth/@flow bodies only)`);
        for (const a of args) this.checkExpr(ctx, a, null);
        return T.unknown;
      }
      const isTable = name === 'curve' || name === 'spline' || name === 'dcurve';
      const seedLast = name === 'noise2' || name === 'ridge'; // imm seed: u32 lane
      args.forEach((a, i) => {
        if (isTable && i === 0) return; // table name: no L1 binding form (spec-issue note)
        const want = seedLast && i === args.length - 1 ? T.u32 : T.fx16;
        const t = this.checkExpr(ctx, a, want);
        if (!tAgree(t, want) && !tAgree(t, T.angle16) && !tAgree(t, T.unknown)) {
          this.sink.error('FORM-E-658', a.span,
            `field-dialect operand must be ${typeName(want)}, got ${typeName(t)} (Q16.16 lanes only, FORM-E-658)`);
        }
      });
      return T.fx16;
    }

    this.sink.error('FORM-E-203', e.span, `call to unknown intrinsic '${name}'`);
    for (const a of args) this.checkExpr(ctx, a, null);
    return T.unknown;
  }

  // ---------------------------------------------------------------------------
  // Field dialect (§6)
  // ---------------------------------------------------------------------------

  private checkField(ms: ModuleSym, d: FieldDecl): void {
    const profile = d.profile === 'earth' || d.profile === 'flow' ? d.profile : null;
    if (!profile) return; // E-650/E-715 already raised at parse

    const ceiling = profile === 'earth' ? 32 : 48;
    if (d.maxOps > BigInt(ceiling)) {
      this.sink.error('FORM-E-654', d.span,
        `declared max_ops ${d.maxOps} above the ${profile} ceiling ${ceiling} (field-ir.md §7.3, FORM-E-654)`);
    }

    // footprint/profile match (E-666)
    if (profile === 'earth' && d.footprint.kind === 'none') {
      this.sink.error('FORM-E-666', d.span, '@earth requires a footprint (rect/circle/capsule), not none (FORM-E-666)');
    }
    if (profile === 'flow' && d.footprint.kind !== 'none') {
      this.sink.error('FORM-E-666', d.span, '@flow requires footprint none (FORM-E-666)');
    }
    // footprint args must be constant fx16 expressions
    if (d.footprint.kind !== 'none') {
      for (const a of d.footprint.args) {
        this.checkExpr(this.ctxFor(ms, 'fn'), a, T.fx16);
        if (this.constEvalFx(ms, a) === null && this.constEval(ms, a) === null) {
          this.sink.error('FORM-E-308', a.span,
            'footprint argument must be a constant expression (the envelope is recorded in the command and cost report)');
        }
      }
    }

    // params struct laws (E-660/661/662)
    let paramsStruct: StructDecl | null = null;
    if (d.paramsStruct) {
      const r = this.resolveUnqualified(ms, d.paramsStruct);
      if (!r || r.ambiguous === true || r.sym.kind !== 'struct') {
        this.sink.error('FORM-E-662', d.paramsSpan!,
          `params binding '${d.paramsStruct}' is missing or not a struct (FORM-E-662)`);
      } else {
        paramsStruct = r.sym.decl as unknown as StructDecl;
        const maxFields = profile === 'earth' ? 8 : 4;
        if (paramsStruct.fields.length > maxFields) {
          this.sink.error('FORM-E-661', d.paramsSpan!,
            `params struct '${d.paramsStruct}' has ${paramsStruct.fields.length} fields — ${profile} packs at most ${maxFields} p-lanes (FORM-E-661)`);
        }
        for (const f of paramsStruct.fields) {
          if (f.type.kind !== 'named' || f.type.name !== 'fx16') {
            this.sink.error('FORM-E-660', f.span,
              `params struct field '${f.name}' must be fx16 (p-lanes are Q16.16, FORM-E-660)`);
          }
        }
      }
    }

    // dialect context: lanes + params bindings (§6.2)
    const ctx = this.ctxFor(ms, 'field');
    ctx.domain = 'field';
    ctx.profile = profile;
    ctx.fieldDecl = d;
    const laneStruct: Record<string, Type> = profile === 'earth'
      ? { sample: { t: 'struct', name: '__earth_sample' } }
      : { p: { t: 'struct', name: '__flow_p' } };
    for (const [k, v] of Object.entries(laneStruct)) {
      ctx.locals.set(k, { type: v, letSpan: d.span, assigned: true, isParam: true });
    }
    if (paramsStruct) {
      ctx.locals.set('params', { type: { t: 'struct', name: paramsStruct.name }, letSpan: d.span, assigned: true, isParam: true });
    }

    // body: dialect expression surface
    let opCount = 0n;
    const lets: { name: string; span: SourceSpan; expr: Expr }[] = [];
    for (const s of d.body) {
      if (s.kind === 'field_let') {
        const name = s.name;
        const ty = this.checkFieldExpr(ctx, s.value, null);
        if (!tAgree(ty, T.fx16) && !tAgree(ty, T.angle16) && !tAgree(ty, T.u32) && !tAgree(ty, T.bool) && !tAgree(ty, T.unknown)) {
          this.sink.error('FORM-E-658', s.value.span,
            `field local '${s.name}' has type ${typeName(ty)} — every lane/local is fx16 in the field dialect (FORM-E-658)`);
        }
        ctx.locals.set(name, { type: ty, letSpan: s.span, assigned: true, isParam: false });
        opCount += this.fieldOpCount(s.value);
        lets.push({ name, span: s.span, expr: s.value });
      } else {
        for (const rf of (s as Extract<FieldStmt, { kind: 'field_return' }>).fields) {
          this.checkFieldExpr(ctx, rf.value, T.fx16);
          opCount += this.fieldOpCount(rf.value);
        }
      }
    }

    // return record (E-663) — checked after the body so lets are in scope
    const wantRecord = profile === 'earth' ? 'terrain_delta' : 'flow_update';
    const retStmt = d.body.find((s) => s.kind === 'field_return');
    if (!retStmt) {
      this.sink.error('FORM-E-663', d.span,
        `field body has no return ${wantRecord} { ... } statement (FORM-E-663)`);
    } else {
      const rs = retStmt as Extract<FieldStmt, { kind: 'field_return' }>;
      if (rs.record !== wantRecord) {
        this.sink.error('FORM-E-663', rs.span,
          `@${profile} returns ${wantRecord}, got '${rs.record}' (FORM-E-663)`);
      }
      this.checkFieldRecordCtx(ctx, rs.record, rs.fields);
    }

    // E-659: register budget — syntactic liveness over the let list; the
    // return record is a final consumer (checked before the op count so both
    // codes surface independently)
    const returnExprs = retStmt && retStmt.kind === 'field_return'
      ? (retStmt as Extract<FieldStmt, { kind: 'field_return' }>).fields.map((f) => f.value)
      : [];
    const live = this.maxLiveLets(lets, returnExprs);
    if (live > 64) {
      this.sink.error('FORM-E-659', d.span,
        `${live} simultaneously-live values — the register budget is 64, no spilling (field-ir.md §11.2, FORM-E-659)`);
    }

    // E-655: lowered instruction count above declared max_ops (syntactic
    // estimate; the W3.4 lowering pins the true count)
    if (opCount > d.maxOps) {
      this.sink.error('FORM-E-655', d.span,
        `field body lowers to ~${opCount} instructions, above the declared max_ops ${d.maxOps} (FORM-E-655)`);
    }
  }

  private checkFieldRecordCtx(ctx: Ctx, record: string, fields: { name: string; value: Expr; span: SourceSpan }[]): void {
    const SHAPE: Record<string, Record<string, Type>> = {
      terrain_delta: { height: T.fx16, velocity: T.fx16, material: T.u32, nav_cost: T.fx16 },
      flow_update: { x: T.fx16, y: T.fx16, z: T.fx16, vx: T.fx16, vy: T.fx16, vz: T.fx16, attr0: T.fx16 },
    };
    const shape = SHAPE[record];
    if (!shape) return;
    const seen = new Set<string>();
    for (const f of fields) {
      const want = shape[f.name];
      if (!want) {
        this.sink.error('FORM-E-104', f.span,
          `record '${record}' has no field '${f.name}' (FORM-E-104)`);
        continue;
      }
      if (seen.has(f.name)) this.sink.error('FORM-E-106', f.span, `duplicate field '${f.name}' (FORM-E-106)`);
      seen.add(f.name);
      this.checkFieldExpr(ctx, f.value, want);
    }
    for (const name of Object.keys(shape)) {
      if (!seen.has(name)) {
        this.sink.error('FORM-E-105', fields[0]?.span ?? ctx.mod.ast.span,
          `record '${record}' omits field '${name}' (all lanes required, FORM-E-105)`);
      }
    }
  }

  private checkFieldRecord(ms: ModuleSym, record: string, fields: { name: string; value: Expr; span: SourceSpan }[]): void {
    const ctx = this.ctxFor(ms, 'field');
    ctx.domain = 'field';
    this.checkFieldRecordCtx(ctx, record, fields);
  }

  /** Expression checking inside the dialect: admission (E-665) + fx16 lanes. */
  private checkFieldExpr(ctx: Ctx, e: Expr, expected: Type | null): Type {
    // refused forms first (message cites the form)
    switch (e.kind) {
      case 'string':
        this.sink.error('FORM-E-665', e.span, 'string in a field body (not admitted, FORM-E-665)');
        return T.unknown;
      case 'binary':
        if (['/', '&&', '||', '&', '|', '^', '<<', '>>'].includes(e.op)) {
          this.sink.error('FORM-E-665', e.span,
            e.op === '/'
              ? "'/' is not admitted in the field dialect — use rcp(x) (§6.3, FORM-E-665)"
              : `operator '${e.op}' is not admitted in the field dialect (§6.3 surface, FORM-E-665)`);
        }
        break;
      case 'unary':
        if (e.op === '~' || e.op === '!') {
          this.sink.error('FORM-E-665', e.span,
            `unary '${e.op}' is not admitted in the field dialect (comparisons feed SELECT, FORM-E-665)`);
        }
        break;
      default:
        break;
    }
    // state/enum access refusals
    if (e.kind === 'ident' && !ctx.locals.has(e.name)) {
      const r = this.resolveUnqualified(ctx.mod, e.name);
      if (r && r.ambiguous !== true && (r.sym.kind === 'pool' || r.sym.kind === 'global')) {
        this.sink.error('FORM-E-656', e.span,
          `state access '${e.name}' inside a field body (no state access, FORM-E-656)`);
        return T.unknown;
      }
      if (r && r.ambiguous !== true && r.sym.kind === 'fn') {
        this.sink.error('FORM-E-652', e.span, `call to fn '${e.name}' inside a field body (FORM-E-652)`);
        return T.unknown;
      }
    }
    const ty = this.checkExpr(ctx, e, expected);
    // fx24 / world / velocity are Q2 refusals (E-332)
    if (['fx24', 'world2', 'world3', 'velocity3'].includes(ty.t)) {
      this.sink.error('FORM-E-332', e.span,
        `${typeName(ty)} inside a field declaration — Field IR lanes are Q16.16 (fx24 never in field programs, Q2/FORM-E-332)`);
      return T.unknown;
    }
    return ty;
  }

  /** Syntactic op estimate per the §6.3 table (one builder call per row). */
  private fieldOpCount(e: Expr): bigint {
    switch (e.kind) {
      case 'literal': return 1n;
      case 'ident': return 1n;
      case 'member': return 1n;
      case 'unary': return this.fieldOpCount(e.operand) + 1n;
      case 'binary': {
        const l = this.fieldOpCount(e.l);
        const r = this.fieldOpCount(e.r);
        if (e.op === '+' && (e.l.kind === 'binary' && e.l.op === '*' || e.r.kind === 'binary' && e.r.op === '*')) {
          return l + r - 1n; // a*b+c fuses to MAD
        }
        return l + r + 1n;
      }
      case 'if_expr': {
        return this.fieldOpCount(e.cond) + 1n + this.fieldOpCount(e.then) + this.fieldOpCount(e.else) + 1n;
      }
      case 'call': {
        let sum = 1n;
        for (const a of e.args) sum += this.fieldOpCount(a);
        return sum;
      }
      case 'record': {
        let sum = 0n;
        for (const f of e.fields) sum += this.fieldOpCount(f.value);
        return sum;
      }
      default:
        return 1n;
    }
  }

  /** Max simultaneously-live locals across the straight-line let list. */
  private maxLiveLets(lets: { name: string; span: SourceSpan; expr: Expr }[], tailConsumers: Expr[] = []): number {
    const names = lets.map((l) => l.name);
    const uses: number[][] = names.map(() => []);
    const collect = (expr: Expr, emit: (index: number) => void): void => {
      if (expr.kind === 'ident') {
        const i = names.indexOf(expr.name);
        if (i >= 0) emit(i);
      } else if (expr.kind === 'binary') { collect(expr.l, emit); collect(expr.r, emit); }
      else if (expr.kind === 'unary') collect(expr.operand, emit);
      else if (expr.kind === 'if_expr') { collect(expr.cond, emit); collect(expr.then, emit); collect(expr.else, emit); }
      else if (expr.kind === 'call') for (const a of expr.args) collect(a, emit);
      else if (expr.kind === 'member') collect(expr.obj, emit);
      else if (expr.kind === 'index') { collect(expr.obj, emit); collect(expr.index, emit); }
      else if (expr.kind === 'record') for (const f of expr.fields) collect(f.value, emit);
    };
    lets.forEach((l, i) => {
      const u = uses[i]!;
      collect(l.expr, (j) => u.push(j));
    });
    // tail consumers (the return record) use names after every let
    const tailUses = new Set<number>();
    for (const tc of tailConsumers) collect(tc, (j) => tailUses.add(j));
    const usedAfter = (j: number, from: number): boolean =>
      uses.slice(from).some((u) => u.includes(j)) || tailUses.has(j);
    let max = 0;
    for (let i = 0; i < lets.length; i++) {
      let live = 1;
      for (let j = 0; j < lets.length; j++) {
        if (j === i) continue;
        if (j < i && usedAfter(j, i)) live++;
      }
      if (live > max) max = live;
    }
    return max;
  }

  /** fx-valued constant eval (for footprint args); returns raw fx16 int or null. */
  private constEvalFx(ms: ModuleSym, e: Expr): bigint | null {
    if (e.kind === 'literal' && e.lit === 'frac') {
      const f = e.frac!;
      const num = BigInt(f.intDigits + (f.fracDigits || ''));
      const den = 10n ** BigInt(f.fracDigits.length || 0);
      const raw = (num << 16n) / den;
      return (num << 16n) % den === 0n ? raw : null;
    }
    return this.constEval(ms, e) === null ? null : (this.constEval(ms, e)! << 16n);
  }
}

/** All let names in a statement tree (use-before-let detection, E-303). */
function collectLetNames(stmts: Stmt[]): Set<string> {
  const names = new Set<string>();
  const walk = (ss: Stmt[]): void => {
    for (const s of ss) {
      if (s.kind === 'let') names.add(s.name);
      else if (s.kind === 'if') { walk(s.then); if (Array.isArray(s.else)) walk(s.else); else if (s.else) walk([s.else]); }
      else if (s.kind === 'for') walk(s.body);
    }
  };
  walk(stmts);
  return names;
}

/** Built-in button bit consts (input_rules.md §4, frozen 32 bits 0..15). */
const BUILTIN_CONSTS: ReadonlyMap<string, bigint> = new Map([
  ['BTN_UP', 0n], ['BTN_DOWN', 1n], ['BTN_LEFT', 2n], ['BTN_RIGHT', 3n],
  ['BTN_A', 4n], ['BTN_B', 5n], ['BTN_X', 6n], ['BTN_Y', 7n],
  ['BTN_L2', 8n], ['BTN_R2', 9n], ['BTN_L1', 10n], ['BTN_R1', 11n],
  ['BTN_L3', 12n], ['BTN_R3', 13n], ['BTN_SELECT', 14n], ['BTN_START', 15n],
]);

/** §4.6 intrinsics invoked by bare name (the builtin table wins over user fns). */
const BARE_INTRINSICS = new Set([
  'abs', 'min', 'max', 'clamp', 'sin', 'cos', 'atan2_approx', 'sqrt_approx',
  'to_fx16', 'to_fx24', 'to_unit8', 'to_angle16',
  'dot2', 'dot3', 'length', 'normalize', 'mix',
  // field-dialect op table (§6.3) — only admitted inside field bodies
  'dot2', 'dist', 'length2', 'length3', 'normalize2', 'normalize3', 'rcp',
  'curve', 'spline', 'dcurve', 'noise2', 'ridge', 'ring', 'rot2', 'rot3',
  'smoothstep',
]);

/** Sentinel expression for missing arguments (keeps messages anchored). */
function none(span: SourceSpan): Expr {
  return { kind: 'literal', lit: 'int', text: '<missing>', intVal: 0n, span };
}

function isBareInt(expr: Expr | undefined): expr is Extract<Expr, { kind: 'literal' }> {
  return expr?.kind === 'literal' && expr.lit === 'int';
}

function adoptableLiteralType(type: Type): boolean {
  return type.t === 'u32' || type.t === 'fx16' || type.t === 'fx24'
    || type.t === 'angle16' || type.t === 'unit8';
}

function firstMemberAfterRoot(expr: Expr, root: string): string | null {
  if (expr.kind === 'member') {
    if (expr.obj.kind === 'ident' && expr.obj.name === root) return expr.field;
    return firstMemberAfterRoot(expr.obj, root);
  }
  if (expr.kind === 'index') return firstMemberAfterRoot(expr.obj, root);
  return null;
}

/** The enclosing fn name for call-graph edges (threaded via ctx by checkFn). */
const currentFnCtx = new WeakMap<Ctx, string>();
function currentFn(ctx: Ctx): string {
  return currentFnCtx.get(ctx) ?? '<top>';
}
export function setCurrentFn(ctx: Ctx, name: string): void {
  currentFnCtx.set(ctx, name);
}
