// lower.ts — W3.2 checked AST -> resolved HIR (W3.3, D3).
// This pass is never entered after an error diagnostic; it does not create a
// second checker. It resolves the already-admitted names/types for lowering.

import type {
  Access, Expr, FieldDecl, ModuleAst, RecordLit, ScenarioItem, Stmt, SystemDecl,
  TopDecl, TypeExpr,
} from '../frontend/ast.js';
import { serializeAst } from '../frontend/ast.js';
import type { FrontendResult } from '../frontend/index.js';
import { T, typeName, type Type } from '../frontend/checker.js';
import type { SourceSpan } from '../frontend/span.js';
import { crc32c } from '../generated/abi.js';
import {
  type HirDeclaration, type HirExpr, type HirField, type HirFunction,
  type HirGlobal, type HirModule, type HirPool, type HirProgram,
  type HirSourceRow, type HirStmt, type HirStruct, type HirSymbolRef,
} from './model.js';

export const SOURCE_KIND_FIELD = 3;
export const SOURCE_KIND_SYSTEM = 8;
export const SOURCE_KIND_EMIT = 9;
export const SOURCE_KIND_POOL = 10;
export const SOURCE_KIND_SCENARIO = 11;

interface ModuleInfo {
  ast: ModuleAst;
  index: number;
  table: Map<string, SymbolInfo>;
}

interface SymbolInfo {
  module: number;
  order: number;
  name: string;
  decl: TopDecl;
}

type LocalEnv = Map<string, Type>;

const BUTTONS = new Map<string, bigint>([
  ['BTN_UP', 0n], ['BTN_DOWN', 1n], ['BTN_LEFT', 2n], ['BTN_RIGHT', 3n],
  ['BTN_A', 4n], ['BTN_B', 5n], ['BTN_X', 6n], ['BTN_Y', 7n],
  ['BTN_L2', 8n], ['BTN_R2', 9n], ['BTN_L1', 10n], ['BTN_R1', 11n],
  ['BTN_L3', 12n], ['BTN_R3', 13n], ['BTN_SELECT', 14n], ['BTN_START', 15n],
]);

/** Returns null rather than emitting any partial IR after frontend errors. */
export function lowerHir(frontend: FrontendResult): HirProgram | null {
  if (!frontend.ok || frontend.check?.schedule === null || frontend.check === null) return null;
  return new HirLowerer(
    frontend.modules,
    frontend.check.schedule,
    frontend.check.expressionTypes,
    frontend.check.accessKeys,
  ).run();
}

class HirLowerer {
  private readonly modules: ModuleInfo[] = [];
  private readonly moduleByName = new Map<string, number>();
  private readonly declarations: HirDeclaration[] = [];
  private readonly sourceIds: HirSourceRow[] = [];
  private readonly sourceIndex = new Map<number, number>();
  private populationIndex = 0;
  private soundIndex = 0;
  private nextRngSlot = 0;

  constructor(
    private readonly asts: ModuleAst[],
    private readonly schedule: NonNullable<FrontendResult['check']>['schedule'] & {},
    private readonly expressionTypes: ReadonlyMap<Expr, Type>,
    private readonly accessKeys: ReadonlyMap<Access, string>,
  ) {}

  run(): HirProgram {
    const ordered = [...this.asts].sort((a, b) => byteCompare(a.span.file, b.span.file));
    ordered.forEach((ast, index) => {
      const info: ModuleInfo = { ast, index, table: new Map() };
      this.modules.push(info);
      this.moduleByName.set(ast.name, index);
    });
    for (const mod of this.modules) {
      let order = 0;
      for (const decl of mod.ast.decls) {
        if (decl.kind === 'BadDecl') continue;
        mod.table.set(decl.name, { module: mod.index, order: order++, name: decl.name, decl });
      }
    }
    for (const mod of this.modules) {
      let order = 0;
      for (const decl of mod.ast.decls) {
        if (decl.kind !== 'BadDecl') this.declarations.push(this.declaration(mod.index, order++, decl));
      }
    }
    this.validateSourceIds();
    this.sourceIds.sort((a, b) => a.sourceId - b.sourceId);
    const modules: HirModule[] = this.modules.map((m) => ({
      index: m.index,
      name: m.ast.name,
      file: m.ast.span.file,
      imports: m.ast.imports.map((i) => ({
        module: this.moduleByName.get(i.module)!, names: [...i.names], span: i.span,
      })),
      span: m.ast.span,
    }));
    const manifest = this.manifestBytes();
    return {
      modules,
      declarations: this.declarations,
      schedule: this.schedule,
      sourceIds: this.sourceIds,
      rngSlotCount: this.nextRngSlot,
      manifestCrc32c: crc32c(0, manifest),
    };
  }

  private declaration(module: number, order: number, decl: Exclude<TopDecl, { kind: 'BadDecl' }>): HirDeclaration {
    switch (decl.kind) {
      case 'Const': {
        const type = this.type(module, decl.type);
        return {
          kind: 'const', domain: 'constant', module, order, name: decl.name, type,
          init: this.expr(module, decl.init, type, new Map()),
          raw: this.raw(module, decl.init, type), span: decl.span,
        };
      }
      case 'Enum': {
        let next = 0n;
        return {
          kind: 'enum', domain: 'constant', module, order, name: decl.name,
          members: decl.members.map((m) => {
            const value = m.value ?? next;
            next = value + 1n;
            return { name: m.name, value, span: m.span };
          }),
          span: decl.span,
        };
      }
      case 'Struct':
        return {
          kind: 'struct', domain: 'constant', module, order, name: decl.name,
          fields: decl.fields.map((f) => ({ name: f.name, type: this.type(module, f.type), span: f.span })),
          span: decl.span,
        };
      case 'Pool': {
        const sym = this.require(module, decl.structName);
        const capacity = decl.capacity?.kind === 'int'
          ? this.u32Number(decl.capacity.value, `pool '${decl.name}' capacity`, true)
          : this.u32Number(this.intConst(module, decl.capacity?.name ?? '') ?? 0n,
            `pool '${decl.name}' capacity`, true);
        const elementType: Type = { t: 'struct', name: qname(sym.module, sym.name) };
        return {
          kind: 'pool', domain: 'state', module, order, name: decl.name,
          structModule: sym.module, structName: sym.name, capacity,
          elementBytes: this.typeBytes(elementType), populationIndex: this.populationIndex++,
          sourceId: this.sourceId(SOURCE_KIND_POOL, module, decl.name, decl.span), span: decl.span,
        };
      }
      case 'Global': {
        const type = this.type(module, decl.type);
        return {
          kind: 'global', domain: 'state', module, order, name: decl.name, type,
          init: this.expr(module, decl.init, type, new Map()), span: decl.span,
        };
      }
      case 'Fn': {
        const env: LocalEnv = new Map();
        const params = decl.params.map((p) => {
          const type = this.type(module, p.type);
          env.set(p.name, type);
          return { name: p.name, type, span: p.span };
        });
        const fn: HirFunction = {
          kind: 'fn', domain: 'pure', module, order, name: decl.name, params,
          returnType: this.type(module, decl.ret), body: [], span: decl.span,
        };
        fn.body = this.stmts(module, decl.body, env, 'pure');
        return fn;
      }
      case 'System': {
        const stagger = decl.staggerPool === null ? null : this.require(module, decl.staggerPool);
        return {
          kind: 'system', domain: 'sim', module, order, name: decl.name,
          every: this.u32Number(decl.every, `system '${decl.name}' rate`, true),
          staggerRate: decl.staggerPool === null ? null
            : this.u32Number(decl.staggerRate ?? decl.every, `system '${decl.name}' stagger rate`, true),
          staggerPool: stagger ? { module: stagger.module, name: stagger.name } : null,
          reads: decl.reads.map((access) => this.requireAccessKey(access)),
          writes: decl.writes.map((access) => this.requireAccessKey(access)),
          body: this.stmts(module, decl.body, new Map(), 'sim'),
          sourceId: this.sourceId(SOURCE_KIND_SYSTEM, module, decl.name, decl.span), span: decl.span,
        };
      }
      case 'Field':
        return this.field(module, order, decl);
      case 'Presentation': {
        const views = [];
        let sharedBudgetPct = 0;
        const emits = [];
        for (const item of decl.items) {
          if (item.kind === 'view') {
            views.push({
              id: Number(item.id), camera: item.camera ? this.expr(module, item.camera, T.world3, new Map()) : null,
              budgetPct: Number(item.budgetPct), span: item.span,
            });
          } else if (item.kind === 'shared_budget') {
            sharedBudgetPct = Number(item.pct);
          } else if (item.kind === 'emit') {
            emits.push({
              kind: 'emit' as const, emitKind: item.emitKind,
              args: item.args.map((a) => ({ name: a.name, value: this.expr(module, a.value, null, new Map()), span: a.span })),
              sourceId: this.sourceId(SOURCE_KIND_EMIT, module, `${decl.name}.${item.emitKind}`, item.span),
              span: item.span,
            });
          }
        }
        return { kind: 'presentation', domain: 'present', module, order, name: decl.name, views, sharedBudgetPct, emits, span: decl.span };
      }
      case 'Scenario':
        return {
          kind: 'scenario', domain: 'test', module, order, name: decl.name,
          items: decl.items.map((item) => ({ ast: item, expressions: this.scenarioExpressions(module, item), span: item.span })),
          sourceId: this.sourceId(SOURCE_KIND_SCENARIO, module, decl.name, decl.span), span: decl.span,
        };
      case 'Sound':
        return {
          kind: 'sound', domain: 'present', module, order, name: decl.name,
          sample: decl.sample,
          params: decl.params.map((p) => ({ kind: p.kind, value: this.expr(module, p.value, null, new Map()), span: p.span })),
          eventIndex: this.soundIndex++, span: decl.span,
        };
    }
  }

  private field(module: number, order: number, decl: FieldDecl): HirField {
    const raw = decl.footprint.kind === 'none' ? [] : decl.footprint.args.map((a) => this.raw(module, a, T.fx16) ?? 0n);
    let rect: [bigint, bigint, bigint, bigint] = [0n, 0n, 0n, 0n];
    if (decl.footprint.kind === 'rect' && raw.length >= 4) rect = [raw[0]!, raw[1]!, raw[2]!, raw[3]!];
    if (decl.footprint.kind === 'circle' && raw.length >= 3) rect = [raw[0]! - raw[2]!, raw[1]! - raw[2]!, raw[0]! + raw[2]!, raw[1]! + raw[2]!];
    if (decl.footprint.kind === 'capsule' && raw.length >= 5) {
      rect = [min(raw[0]!, raw[2]!) - raw[4]!, min(raw[1]!, raw[3]!) - raw[4]!, max(raw[0]!, raw[2]!) + raw[4]!, max(raw[1]!, raw[3]!) + raw[4]!];
    }
    const paramsSym = decl.paramsStruct ? this.require(module, decl.paramsStruct) : null;
    const env: LocalEnv = new Map();
    env.set(decl.profile === 'flow' ? 'p' : 'sample', { t: 'struct', name: decl.profile === 'flow' ? '__flow_p' : '__earth_sample' });
    if (paramsSym) env.set('params', { t: 'struct', name: qname(paramsSym.module, paramsSym.name) });
    return {
      kind: 'field', domain: 'field', module, order, name: decl.name,
      profile: decl.profile === 'flow' ? 'flow' : 'earth',
      params: paramsSym ? { module: paramsSym.module, name: paramsSym.name } : null,
      footprint: { kind: decl.footprint.kind, raw, rect },
      maxOps: this.u32Number(decl.maxOps, `field '${decl.name}' max_ops`),
      body: decl.body.map((s) => {
        const expressions = s.kind === 'field_let'
          ? [this.expr(module, s.value, null, env)]
          : s.fields.map((f) => this.expr(module, f.value, null, env));
        if (s.kind === 'field_let') env.set(s.name, expressions[0]!.type);
        return { ast: s, expressions, span: s.span };
      }),
      sourceId: this.sourceId(SOURCE_KIND_FIELD, module, decl.name, decl.span), span: decl.span,
    };
  }

  private stmts(module: number, body: Stmt[], input: LocalEnv, domain: 'pure' | 'sim'): HirStmt[] {
    const env = input;
    return body.map((stmt) => {
      const expressions: HirExpr[] = [];
      let nested: HirStmt[] = [];
      let elseBody: HirStmt[] = [];
      switch (stmt.kind) {
        case 'let': {
          const expected = stmt.type ? this.type(module, stmt.type) : null;
          const value = this.expr(module, stmt.init, expected, env);
          expressions.push(value);
          env.set(stmt.name, expected ?? value.type);
          break;
        }
        case 'assign': expressions.push(this.expr(module, stmt.target, null, env), this.expr(module, stmt.value, null, env)); break;
        case 'if': {
          expressions.push(this.expr(module, stmt.cond, T.bool, env));
          nested = this.stmts(module, stmt.then, new Map(env), domain);
          if (Array.isArray(stmt.else)) elseBody = this.stmts(module, stmt.else, new Map(env), domain);
          else if (stmt.else) elseBody = this.stmts(module, [stmt.else], new Map(env), domain);
          break;
        }
        case 'for': {
          const loopEnv = new Map(env);
          if (stmt.range.kind === 'range') {
            expressions.push(this.expr(module, stmt.range.lo, T.u32, env), this.expr(module, stmt.range.hi, T.u32, env));
            loopEnv.set(stmt.varName, T.u32);
          } else {
            const pool = this.require(module, stmt.range.pool);
            expressions.push(this.syntheticPoolCount(stmt.range.poolSpan, pool));
            const pd = pool.decl.kind === 'Pool' ? pool.decl : null;
            const st = pd ? this.require(pool.module, pd.structName) : pool;
            loopEnv.set(stmt.varName, { t: 'struct', name: qname(st.module, st.name) });
          }
          nested = this.stmts(module, stmt.body, loopEnv, domain);
          break;
        }
        case 'call_stmt': expressions.push(this.expr(module, stmt.call, null, env)); break;
        case 'spawn': expressions.push(this.expr(module, stmt.value, null, env)); break;
        case 'kill': expressions.push(this.expr(module, stmt.index, T.u32, env)); break;
        case 'return': if (stmt.value) expressions.push(this.expr(module, stmt.value, null, env)); break;
        case 'apply': {
          for (const arg of stmt.args) expressions.push(this.expr(module, arg.value, null, env));
          expressions.push(this.expr(module, stmt.duration, T.u32, env));
          break;
        }
        case 'bad_stmt': break;
      }
      return { ast: stmt, domain, expressions, body: nested, elseBody, span: stmt.span };
    });
  }

  private expr(
    module: number,
    ast: Expr,
    expected: Type | null,
    env: LocalEnv,
    forcePoolColumn = false,
  ): HirExpr {
    const children: HirExpr[] = [];
    let type: Type = T.unknown;
    let symbol: HirSymbolRef | null = null;
    let poolColumn: HirExpr['poolColumn'] = null;
    switch (ast.kind) {
      case 'literal': type = this.literalType(ast, expected); break;
      case 'string': type = T.unknown; break;
      case 'ident': {
        const local = env.get(ast.name);
        if (local) { type = local; symbol = { kind: 'local', module: null, name: ast.name }; break; }
        if (BUTTONS.has(ast.name)) { type = T.u32; symbol = { kind: 'const', module: null, name: ast.name }; break; }
        const sym = this.resolve(module, ast.name);
        if (sym) { type = this.symbolType(sym); symbol = this.symbolRef(sym); }
        break;
      }
      case 'member': {
        const enumMember = this.enumMember(module, ast, env);
        if (enumMember) {
          type = { t: 'enum', name: qname(enumMember.module, enumMember.name) };
          symbol = { kind: 'enum', module: enumMember.module, name: `${enumMember.name}.${ast.field}` };
          break;
        }
        const pool = this.directPoolRoot(module, ast.obj, env);
        if (pool?.decl.kind === 'Pool') {
          const struct = this.require(pool.module, pool.decl.structName);
          const authored = struct.decl.kind === 'Struct'
            && struct.decl.fields.some((field) => field.name === ast.field);
          if (ast.field === 'count' && !forcePoolColumn) {
            type = T.u32;
            symbol = { kind: 'pool', module: pool.module, name: pool.name };
          } else if (authored) {
            type = this.structFieldType(struct, ast.field);
            poolColumn = { module: pool.module, pool: pool.name, field: ast.field };
          }
          break;
        }
        const qualified = ast.obj.kind === 'ident' && !env.has(ast.obj.name)
          ? this.qualifiedMember(module, ast.obj.name, ast.field)
          : null;
        if (qualified) {
          type = this.symbolType(qualified);
          symbol = this.symbolRef(qualified);
          break;
        }
        children.push(this.expr(module, ast.obj, null, env));
        type = this.memberType(children[0]!.type, ast.field);
        break;
      }
      case 'index': {
        const directColumn = this.directPoolColumn(module, ast.obj, env) !== null;
        children.push(
          this.expr(module, ast.obj, null, env, directColumn),
          this.expr(module, ast.index, T.u32, env),
        );
        type = children[0]!.type.t === 'array' ? children[0]!.type.elem : children[0]!.type;
        break;
      }
      case 'call': {
        const calleeName = callName(ast.callee);
        const callee = this.directDeclaration(module, ast.callee, env);
        children.push(...ast.args.map((argument) => this.expr(module, argument, null, env)));
        if (callee?.decl.kind === 'Fn') {
          type = this.type(callee.module, callee.decl.ret);
          symbol = this.symbolRef(callee);
        } else if (callee?.decl.kind === 'Field' || callee?.decl.kind === 'System') {
          type = T.void;
          symbol = this.symbolRef(callee);
        } else {
          type = intrinsicType(calleeName, children, expected);
          symbol = { kind: 'intrinsic', module: null, name: calleeName };
        }
        break;
      }
      case 'unary':
        children.push(this.expr(module, ast.operand, expected, env));
        type = ast.op === '!' ? T.bool : children[0]!.type;
        break;
      case 'binary':
        children.push(this.expr(module, ast.l, null, env), this.expr(module, ast.r, null, env));
        type = isCompare(ast.op) || ast.op === '&&' || ast.op === '||' ? T.bool : adoptLiteralType(ast, children);
        break;
      case 'if_expr':
        children.push(this.expr(module, ast.cond, T.bool, env), this.expr(module, ast.then, expected, env), this.expr(module, ast.else, expected, env));
        type = children[1]!.type;
        break;
      case 'record': {
        const sym = this.resolve(module, ast.typeName);
        type = vectorType(ast.typeName) ?? (sym ? { t: 'struct', name: qname(sym.module, sym.name) } : T.unknown);
        children.push(...ast.fields.map((field) => this.expr(module, field.value, this.memberType(type, field.name), env)));
        symbol = sym ? this.symbolRef(sym) : null;
        break;
      }
      case 'range':
        children.push(this.expr(module, ast.lo, null, env), this.expr(module, ast.hi, null, env));
        type = T.unknown;
        break;
    }
    const checked = this.expressionTypes.get(ast);
    if (!checked) {
      throw new Error(`internal HIR exact-type failure: checker did not type expression at ${ast.span.file}:${ast.span.start}`);
    }
    if (checked.t === 'unknown') {
      throw new Error(`internal HIR exact-type failure: unresolved expression at ${ast.span.file}:${ast.span.start}`);
    }
    type = this.qualifyCheckedType(module, checked);
    if (checked.t === 'field_table' && ast.kind === 'ident') {
      symbol = { kind: 'field_table', module: null, name: ast.name };
    }
    const rngSlot = symbol?.kind === 'intrinsic' && symbol.name === 'random.stream'
      ? this.nextRngSlot++
      : null;
    return { ast, type, symbol, poolColumn, rngSlot, children, span: ast.span };
  }

  private directDeclaration(module: number, expression: Expr, env: LocalEnv): SymbolInfo | null {
    if (expression.kind === 'ident') return env.has(expression.name) ? null : this.resolve(module, expression.name);
    if (expression.kind === 'member' && expression.obj.kind === 'ident' && !env.has(expression.obj.name)) {
      return this.qualifiedMember(module, expression.obj.name, expression.field);
    }
    return null;
  }

  private directPoolRoot(module: number, expression: Expr, env: LocalEnv): SymbolInfo | null {
    const symbol = this.directDeclaration(module, expression, env);
    return symbol?.decl.kind === 'Pool' ? symbol : null;
  }

  private directPoolColumn(module: number, expression: Expr, env: LocalEnv): {
    pool: SymbolInfo;
    field: string;
  } | null {
    if (expression.kind !== 'member') return null;
    const pool = this.directPoolRoot(module, expression.obj, env);
    if (!pool || pool.decl.kind !== 'Pool') return null;
    const struct = this.require(pool.module, pool.decl.structName);
    if (struct.decl.kind !== 'Struct'
        || !struct.decl.fields.some((field) => field.name === expression.field)) return null;
    return { pool, field: expression.field };
  }

  private enumMember(
    module: number,
    expression: Extract<Expr, { kind: 'member' }>,
    env: LocalEnv,
  ): SymbolInfo | null {
    const owner = this.directDeclaration(module, expression.obj, env);
    if (owner?.decl.kind !== 'Enum'
        || !owner.decl.members.some((member) => member.name === expression.field)) return null;
    return owner;
  }

  private scenarioExpressions(module: number, item: ScenarioItem): HirExpr[] {
    if (item.kind === 'at') return [this.expr(module, item.action, null, new Map())];
    if (item.kind === 'assert') return [this.expr(module, item.expr, T.bool, new Map()), ...(item.tolerance ? [this.expr(module, item.tolerance, T.fx16, new Map())] : [])];
    return [];
  }

  private qualifyCheckedType(module: number, type: Type): Type {
    if (type.t === 'array') {
      return { t: 'array', elem: this.qualifyCheckedType(module, type.elem), len: type.len };
    }
    if (type.t === 'struct' || type.t === 'enum') {
      if (type.name.startsWith('__') || type.name.includes('::')) return { t: type.t, name: type.name };
      const owner = type.owner === undefined ? undefined : this.moduleByName.get(type.owner);
      if (type.owner !== undefined && owner === undefined) {
        throw new Error(`internal HIR exact-type failure: unknown owner '${type.owner}' for ${type.t} '${type.name}'`);
      }
      const symbol = owner === undefined
        ? this.resolve(module, type.name)
        : this.modules[owner]!.table.get(type.name) ?? null;
      if (!symbol || (type.t === 'struct' && symbol.decl.kind !== 'Struct')
          || (type.t === 'enum' && symbol.decl.kind !== 'Enum')) {
        throw new Error(`internal HIR exact-type failure: cannot qualify ${type.t} '${type.name}' from owner '${type.owner ?? '<legacy>'}'`);
      }
      return { t: type.t, name: qname(symbol.module, symbol.name) };
    }
    if (type.t === 'pool') {
      const owner = type.owner === undefined ? undefined : this.moduleByName.get(type.owner);
      if (type.owner !== undefined && owner === undefined) {
        throw new Error(`internal HIR exact-type failure: unknown owner '${type.owner}' for pool '${type.name}'`);
      }
      const symbol = owner === undefined
        ? this.resolve(module, type.name)
        : this.modules[owner]!.table.get(type.name) ?? null;
      if (!symbol || symbol.decl.kind !== 'Pool') {
        throw new Error(`internal HIR exact-type failure: cannot qualify pool '${type.name}' from owner '${type.owner ?? '<legacy>'}'`);
      }
      const structOwner = type.structOwner === undefined ? undefined : this.moduleByName.get(type.structOwner);
      if (type.structOwner !== undefined && structOwner === undefined) {
        throw new Error(`internal HIR exact-type failure: unknown owner '${type.structOwner}' for pool element '${type.struct}'`);
      }
      const struct = structOwner === undefined
        ? this.require(symbol.module, symbol.decl.structName)
        : this.modules[structOwner]!.table.get(type.struct) ?? null;
      if (!struct || struct.decl.kind !== 'Struct') {
        throw new Error(`internal HIR exact-type failure: cannot qualify pool element '${type.struct}' from owner '${type.structOwner ?? '<legacy>'}'`);
      }
      return { t: 'pool', name: qname(symbol.module, symbol.name), struct: qname(struct.module, struct.name) };
    }
    return type;
  }

  private type(module: number, ast: TypeExpr): Type {
    if (ast.kind === 'array') {
      const value = ast.len.kind === 'int' ? ast.len.value : this.intConst(module, ast.len.name) ?? 0n;
      const len = this.u32Number(value, 'array length', true);
      return { t: 'array', elem: this.type(module, ast.elem), len };
    }
    const builtin = builtinType(ast.name);
    if (builtin) return builtin;
    const sym = this.require(module, ast.name);
    if (sym.decl.kind === 'Struct') return { t: 'struct', name: qname(sym.module, sym.name) };
    if (sym.decl.kind === 'Enum') return { t: 'enum', name: qname(sym.module, sym.name) };
    return T.unknown;
  }

  private symbolType(sym: SymbolInfo): Type {
    switch (sym.decl.kind) {
      case 'Const': case 'Global': return this.type(sym.module, sym.decl.type);
      case 'Pool': return { t: 'pool', name: sym.name, struct: qname(this.require(sym.module, sym.decl.structName).module, sym.decl.structName) };
      case 'Enum': return { t: 'enum', name: qname(sym.module, sym.name) };
      case 'Struct': return { t: 'struct', name: qname(sym.module, sym.name) };
      case 'Fn': return this.type(sym.module, sym.decl.ret);
      case 'Sound': return T.sound;
      default: return T.void;
    }
  }

  private memberType(type: Type, field: string): Type {
    if (type.t === 'world2' || type.t === 'world3' || type.t === 'velocity3') return T.fx24;
    if (type.t === 'padframe') return field === 'buttons' ? T.u32 : T.i32;
    if (type.t === 'struct') {
      if (type.name === '__earth_sample') return field === 'age' ? T.u32 : T.fx16;
      if (type.name === '__flow_p') return field === 'age' || field === 'seed' ? T.u32 : T.fx16;
      const [m, name] = splitQName(type.name);
      const sym = this.modules[m]?.table.get(name);
      return sym ? this.structFieldType(sym, field) : T.unknown;
    }
    if (type.t === 'array') return type.elem;
    return T.unknown;
  }

  private structFieldType(sym: SymbolInfo, field: string): Type {
    if (sym.decl.kind !== 'Struct') return T.unknown;
    const found = sym.decl.fields.find((f) => f.name === field);
    return found ? this.type(sym.module, found.type) : T.unknown;
  }

  private typeBytes(type: Type): number {
    switch (type.t) {
      case 'unit8': case 'bool': return 1;
      case 'angle16': return 2;
      case 'fx16': case 'i32': case 'u32': case 'colour8': case 'enum': return 4;
      case 'fx24': return 8;
      case 'world2': return 16;
      case 'world3': case 'velocity3': return 24;
      case 'array': return type.len * this.typeBytes(type.elem);
      case 'struct': {
        const [m, name] = splitQName(type.name);
        const sym = this.modules[m]?.table.get(name);
        if (!sym || sym.decl.kind !== 'Struct') return 0;
        return sym.decl.fields.reduce((sum, f) => sum + this.typeBytes(this.type(m, f.type)), 0);
      }
      default: return 0;
    }
  }

  private raw(module: number, ast: Expr, type: Type): bigint | null {
    if (ast.kind === 'unary' && ast.op === '-') {
      const value = this.raw(module, ast.operand, type);
      return value === null ? null : -value;
    }
    if (ast.kind === 'literal') {
      if (ast.lit === 'bool') return ast.text === 'true' ? 1n : 0n;
      if (ast.lit === 'colour') {
        const digits = ast.text.startsWith('#') ? ast.text.slice(1) : ast.text;
        const rgb = BigInt(`0x${digits}`);
        return digits.length === 6 ? 0xff000000n | rgb : rgb;
      }
      if (ast.lit === 'int' || ast.lit === 'tick') {
        const value = ast.intVal ?? 0n;
        return type.t === 'fx16' ? value << 16n : type.t === 'fx24' ? value << 24n : value;
      }
      if (ast.lit === 'frac') {
        const frac = ast.frac!;
        const numerator = BigInt(frac.intDigits + frac.fracDigits);
        const denominator = 10n ** BigInt(frac.fracDigits.length);
        if (type.t === 'fx16') return (numerator << 16n) / denominator;
        if (type.t === 'fx24') return (numerator << 24n) / denominator;
        if (type.t === 'angle16') return ((numerator << 16n) / (denominator * (frac.suffix === 'deg' ? 360n : 1n))) & 0xffffn;
        if (type.t === 'unit8') return min(255n, (numerator * 256n + denominator * 50n) / (denominator * 100n));
      }
    }
    if (ast.kind === 'ident') {
      if (BUTTONS.has(ast.name)) return BUTTONS.get(ast.name)!;
      const sym = this.resolve(module, ast.name);
      if (sym?.decl.kind === 'Const') return this.raw(sym.module, sym.decl.init, this.type(sym.module, sym.decl.type));
    }
    if (ast.kind === 'member') {
      if (ast.obj.kind === 'ident') {
        const qualified = this.qualifiedMember(module, ast.obj.name, ast.field);
        if (qualified?.decl.kind === 'Const') {
          return this.raw(qualified.module, qualified.decl.init, this.type(qualified.module, qualified.decl.type));
        }
        const symbol = this.resolve(module, ast.obj.name);
        if (symbol?.decl.kind === 'Enum') return this.enumRaw(symbol, ast.field);
      }
      if (ast.obj.kind === 'member' && ast.obj.obj.kind === 'ident') {
        const symbol = this.qualifiedMember(module, ast.obj.obj.name, ast.obj.field);
        if (symbol?.decl.kind === 'Enum') return this.enumRaw(symbol, ast.field);
      }
    }
    if (ast.kind === 'binary') {
      const l = this.raw(module, ast.l, type);
      const r = this.raw(module, ast.r, type);
      if (l === null || r === null) return null;
      if (ast.op === '+') return l + r;
      if (ast.op === '-') return l - r;
      if (ast.op === '*') return type.t === 'fx16' ? ((l * r + 0x8000n) >> 16n) : type.t === 'fx24' ? ((l * r + 0x800000n) >> 24n) : l * r;
      if (ast.op === '/' && r !== 0n) return l / r;
      if (ast.op === '<<') return l << r;
      if (ast.op === '>>') return l >> r;
      if (ast.op === '&') return l & r;
      if (ast.op === '|') return l | r;
      if (ast.op === '^') return l ^ r;
    }
    return null;
  }

  private enumRaw(symbol: SymbolInfo, name: string): bigint | null {
    if (symbol.decl.kind !== 'Enum') return null;
    let next = 0n;
    for (const member of symbol.decl.members) {
      const value = member.value ?? next;
      next = value + 1n;
      if (member.name === name) return value;
    }
    return null;
  }

  private intConst(module: number, name: string): bigint | null {
    const sym = this.resolve(module, name);
    return sym?.decl.kind === 'Const' ? this.raw(sym.module, sym.decl.init, this.type(sym.module, sym.decl.type)) : null;
  }

  private resolve(module: number, name: string): SymbolInfo | null {
    const parts = name.split('.');
    if (parts.length === 2) return this.qualifiedMember(module, parts[0]!, parts[1]!);
    if (parts.length !== 1) return null;
    const own = this.modules[module]!.table.get(name);
    if (own) return own;
    for (const imp of this.modules[module]!.ast.imports) {
      if (!imp.names.includes(name)) continue;
      const target = this.moduleByName.get(imp.module);
      const found = target === undefined ? undefined : this.modules[target]!.table.get(name);
      if (found) return found;
    }
    return null;
  }

  private require(module: number, name: string): SymbolInfo {
    const sym = this.resolve(module, name);
    if (!sym) throw new Error(`internal HIR resolution failure: ${this.modules[module]!.ast.name}.${name}`);
    return sym;
  }

  private qualifiedMember(requester: number, moduleName: string, member: string): SymbolInfo | null {
    const module = this.moduleByName.get(moduleName);
    if (module === undefined) return null;
    const importedWhole = module === requester
      || this.modules[requester]!.ast.imports.some((item) => item.module === moduleName && item.names.length === 0);
    return importedWhole ? this.modules[module]!.table.get(member) ?? null : null;
  }

  private symbolRef(sym: SymbolInfo): HirSymbolRef {
    return { kind: symbolKind(sym.decl), module: sym.module, name: sym.name };
  }

  private sourceId(kind: number, module: number, name: string, span: SourceSpan): number {
    const index = this.sourceIndex.get(module) ?? 0;
    if (!Number.isInteger(kind) || kind < 0 || kind > 0xf) {
      throw new Error(`internal source-ID kind ${kind} is outside 4-bit range`);
    }
    if (!Number.isInteger(module) || module < 0 || module > 0xfff) {
      throw new Error(`internal source-ID module ${module} is outside 12-bit range`);
    }
    if (index > 0xffff) {
      throw new Error(`internal source-ID local index ${index} is outside 16-bit range in module ${module}`);
    }
    this.sourceIndex.set(module, index + 1);
    const sourceId = ((kind << 28) | (module << 16) | index) >>> 0;
    this.sourceIds.push({ sourceId, kind, module, file: span.file, span, name, programHash: null });
    return sourceId;
  }

  private validateSourceIds(): void {
    const seen = new Map<number, HirSourceRow>();
    for (const row of this.sourceIds) {
      const kind = row.sourceId >>> 28;
      const module = (row.sourceId >>> 16) & 0xfff;
      const index = row.sourceId & 0xffff;
      if (kind !== row.kind || module !== row.module
          || !Number.isInteger(row.kind) || row.kind < 0 || row.kind > 0xf
          || !Number.isInteger(row.module) || row.module < 0 || row.module > 0xfff
          || index >= (this.sourceIndex.get(row.module) ?? 0)) {
        throw new Error(`internal malformed source ID 0x${row.sourceId.toString(16).padStart(8, '0')} for '${row.name}'`);
      }
      const prior = seen.get(row.sourceId);
      if (prior) {
        throw new Error(`internal duplicate source ID 0x${row.sourceId.toString(16).padStart(8, '0')} for '${prior.name}' and '${row.name}'`);
      }
      seen.set(row.sourceId, row);
    }
  }

  private requireAccessKey(access: Access): string {
    const key = this.accessKeys.get(access);
    if (key === undefined) throw new Error(`internal HIR access key missing for '${access.parts.join('.')}'`);
    return key;
  }

  private u32Number(value: bigint, label: string, positive = false): number {
    if (value < (positive ? 1n : 0n) || value > 0xffffffffn) {
      throw new Error(`internal HIR ${label} ${value} is outside ${positive ? 'positive ' : ''}u32 range`);
    }
    return Number(value);
  }

  private syntheticPoolCount(span: SourceSpan, sym: SymbolInfo): HirExpr {
    const ast: Expr = { kind: 'member', obj: { kind: 'ident', name: sym.name, span }, field: 'count', fieldSpan: span, span };
    return { ast, type: T.u32, symbol: { kind: 'pool', module: sym.module, name: sym.name }, poolColumn: null, rngSlot: null, children: [], span };
  }

  private literalType(ast: Extract<Expr, { kind: 'literal' }>, expected: Type | null): Type {
    if (ast.lit === 'bool') return T.bool;
    if (ast.lit === 'colour') return T.colour8;
    if (ast.lit === 'tick') return T.u32;
    if (ast.lit === 'int') return expected && ['i32', 'u32', 'fx16', 'fx24'].includes(expected.t) ? expected : T.i32;
    const suffix = ast.frac!.suffix;
    if (suffix === 'w') return T.fx24;
    if (suffix === 'turn' || suffix === 'deg') return T.angle16;
    if (suffix === '%') return T.unit8;
    return T.fx16;
  }

  private manifestBytes(): Uint8Array {
    const lines = ['form-program-manifest-v1'];
    for (const mod of this.modules) lines.push(`file ${mod.index} ${mod.ast.span.file}\n${serializeAst(mod.ast).trimEnd()}`);
    const systemOrder = new Map(
      this.declarations
        .filter((decl) => decl.kind === 'system')
        .map((decl) => [`${decl.module}\0${decl.name}`, decl.order]),
    );
    for (const phase of this.schedule.phases) {
      const systems = [...phase.systems].sort((a, b) => {
        const am = this.moduleByName.get(a.module)!;
        const bm = this.moduleByName.get(b.module)!;
        return am - bm || systemOrder.get(`${am}\0${a.name}`)! - systemOrder.get(`${bm}\0${b.name}`)!;
      });
      for (const system of systems) lines.push(`phase ${phase.index} ${system.module}.${system.name} every ${system.every}`);
    }
    return new TextEncoder().encode(lines.join('\n') + '\n');
  }
}

function builtinType(name: string): Type | null {
  return ({
    fx16: T.fx16, fx24: T.fx24, angle16: T.angle16, unit8: T.unit8,
    i32: T.i32, u32: T.u32, bool: T.bool, world2: T.world2,
    world3: T.world3, velocity3: T.velocity3, colour8: T.colour8,
  } as Record<string, Type>)[name] ?? null;
}

function vectorType(name: string): Type | null {
  return name === 'world2' ? T.world2 : name === 'world3' ? T.world3 : name === 'velocity3' ? T.velocity3 : null;
}

function symbolKind(decl: TopDecl): HirSymbolRef['kind'] {
  return decl.kind === 'Const' ? 'const' : decl.kind === 'Enum' ? 'enum' : decl.kind === 'Struct' ? 'struct'
    : decl.kind === 'Pool' ? 'pool' : decl.kind === 'Global' ? 'global' : decl.kind === 'Fn' ? 'fn'
      : decl.kind === 'System' ? 'system' : decl.kind === 'Field' ? 'field'
        : decl.kind === 'Presentation' ? 'presentation' : decl.kind === 'Scenario' ? 'scenario' : 'sound';
}

function callName(expr: Expr): string {
  if (expr.kind === 'ident') return expr.name;
  if (expr.kind === 'member') return `${callName(expr.obj)}.${expr.field}`;
  return '<call>';
}

function intrinsicType(name: string, args: HirExpr[], expected: Type | null): Type {
  if (name === 'input.player') return T.padframe;
  if (name === 'input.held') return T.bool;
  if (name === 'random.stream') return T.stream;
  if (name.startsWith('random.')) return builtinType(name.slice(7)) ?? T.unknown;
  if (name === 'terrain.height' || name === 'sqrt_approx' || name === 'sin' || name === 'cos') return T.fx16;
  if (name === 'atan2_approx') return T.angle16;
  if (name.startsWith('to_')) return builtinType(name.slice(3)) ?? T.unknown;
  if (name === 'dot2' || name === 'dot3') return T.fx24;
  if (name === 'length') return T.fx16;
  if (name === 'normalize' || name === 'mix' || name === 'abs' || name === 'min' || name === 'max' || name === 'clamp') return args[0]?.type ?? expected ?? T.unknown;
  return expected ?? args[0]?.type ?? T.unknown;
}

function adoptLiteralType(ast: Extract<Expr, { kind: 'binary' }>, children: HirExpr[]): Type {
  if (ast.l.kind === 'literal' && ast.l.lit === 'int') return children[1]!.type;
  return children[0]!.type;
}

function isCompare(op: string): boolean { return ['<', '<=', '>', '>=', '==', '!='].includes(op); }
function qname(module: number, name: string): string { return `${module}::${name}`; }
function splitQName(name: string): [number, string] {
  const split = name.indexOf('::');
  if (split < 0) throw new Error(`internal HIR type is not qualified: ${name}`);
  return [Number(name.slice(0, split)), name.slice(split + 2)];
}
function min(a: bigint, b: bigint): bigint { return a < b ? a : b; }
function max(a: bigint, b: bigint): bigint { return a > b ? a : b; }
function byteCompare(a: string, b: string): number { return Buffer.compare(Buffer.from(a), Buffer.from(b)); }

export function describeType(type: Type): string { return typeName(type); }
