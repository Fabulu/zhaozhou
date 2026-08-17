// model.ts — resolved, typed, domain-tagged Form HIR (W3.3, D3).
// Every declaration and expression retains the frontend byte SourceSpan.

import type {
  Expr, FieldStmt, ScenarioItem, Stmt,
} from '../frontend/ast.js';
import type { Schedule, Type } from '../frontend/checker.js';
import type { SourceSpan } from '../frontend/span.js';

export type HirDomain = 'constant' | 'state' | 'pure' | 'sim' | 'field' | 'present' | 'test';
export type HirSymbolKind =
  | 'const' | 'enum' | 'struct' | 'pool' | 'global' | 'fn' | 'system'
  | 'field' | 'presentation' | 'scenario' | 'sound' | 'local' | 'intrinsic'
  | 'field_table';

export interface HirModule {
  index: number;
  name: string;
  file: string;
  imports: { module: number; names: string[]; span: SourceSpan }[];
  span: SourceSpan;
}

export interface HirSymbolRef {
  kind: HirSymbolKind;
  module: number | null;
  name: string;
}

/** A checked expression plus its resolved type/name references. */
export interface HirExpr {
  ast: Expr;
  type: Type;
  symbol: HirSymbolRef | null;
  /** Stable canonical slot owned by a random.stream call site. */
  rngSlot: number | null;
  children: HirExpr[];
  span: SourceSpan;
}

export interface HirConst {
  kind: 'const';
  domain: 'constant';
  module: number;
  order: number;
  name: string;
  type: Type;
  init: HirExpr;
  raw: bigint | null;
  span: SourceSpan;
}

export interface HirEnum {
  kind: 'enum';
  domain: 'constant';
  module: number;
  order: number;
  name: string;
  members: { name: string; value: bigint; span: SourceSpan }[];
  span: SourceSpan;
}

export interface HirStruct {
  kind: 'struct';
  domain: 'constant';
  module: number;
  order: number;
  name: string;
  fields: { name: string; type: Type; span: SourceSpan }[];
  span: SourceSpan;
}

export interface HirPool {
  kind: 'pool';
  domain: 'state';
  module: number;
  order: number;
  name: string;
  structModule: number;
  structName: string;
  capacity: number;
  elementBytes: number;
  populationIndex: number;
  sourceId: number;
  span: SourceSpan;
}

export interface HirGlobal {
  kind: 'global';
  domain: 'state';
  module: number;
  order: number;
  name: string;
  type: Type;
  init: HirExpr;
  span: SourceSpan;
}

export interface HirFunction {
  kind: 'fn';
  domain: 'pure';
  module: number;
  order: number;
  name: string;
  params: { name: string; type: Type; span: SourceSpan }[];
  returnType: Type;
  body: HirStmt[];
  span: SourceSpan;
}

export interface HirStmt {
  ast: Stmt;
  domain: 'pure' | 'sim';
  expressions: HirExpr[];
  body: HirStmt[];
  elseBody: HirStmt[];
  span: SourceSpan;
}

export interface HirSystem {
  kind: 'system';
  domain: 'sim';
  module: number;
  order: number;
  name: string;
  every: number;
  staggerRate: number | null;
  staggerPool: { module: number; name: string } | null;
  reads: string[];
  writes: string[];
  body: HirStmt[];
  sourceId: number;
  span: SourceSpan;
}

export interface HirField {
  kind: 'field';
  domain: 'field';
  module: number;
  order: number;
  name: string;
  profile: 'earth' | 'flow';
  params: { module: number; name: string } | null;
  footprint: { kind: 'none' | 'rect' | 'circle' | 'capsule'; raw: bigint[]; rect: [bigint, bigint, bigint, bigint] };
  maxOps: number;
  body: { ast: FieldStmt; expressions: HirExpr[]; span: SourceSpan }[];
  sourceId: number;
  span: SourceSpan;
}

export interface HirEmit {
  kind: 'emit';
  emitKind: string;
  args: { name: string; value: HirExpr; span: SourceSpan }[];
  sourceId: number;
  span: SourceSpan;
}

export interface HirPresentation {
  kind: 'presentation';
  domain: 'present';
  module: number;
  order: number;
  name: string;
  views: { id: number; camera: HirExpr | null; budgetPct: number; span: SourceSpan }[];
  sharedBudgetPct: number;
  emits: HirEmit[];
  span: SourceSpan;
}

export interface HirScenario {
  kind: 'scenario';
  domain: 'test';
  module: number;
  order: number;
  name: string;
  items: { ast: ScenarioItem; expressions: HirExpr[]; span: SourceSpan }[];
  sourceId: number;
  span: SourceSpan;
}

export interface HirSound {
  kind: 'sound';
  domain: 'present';
  module: number;
  order: number;
  name: string;
  sample: string;
  params: { kind: string; value: HirExpr; span: SourceSpan }[];
  eventIndex: number;
  span: SourceSpan;
}

export type HirDeclaration =
  | HirConst | HirEnum | HirStruct | HirPool | HirGlobal | HirFunction
  | HirSystem | HirField | HirPresentation | HirScenario | HirSound;

export interface HirSourceRow {
  sourceId: number;
  kind: number;
  module: number;
  file: string;
  span: SourceSpan;
  name: string;
  programHash: number | null;
}

export interface HirProgram {
  modules: HirModule[];
  declarations: HirDeclaration[];
  schedule: Schedule;
  sourceIds: HirSourceRow[];
  /** Number of stable random.stream call-site slots across all domains. */
  rngSlotCount: number;
  manifestCrc32c: number;
}

export function declarationsOf<K extends HirDeclaration['kind']>(
  program: HirProgram,
  kind: K,
): Extract<HirDeclaration, { kind: K }>[] {
  return program.declarations.filter((d): d is Extract<HirDeclaration, { kind: K }> => d.kind === kind);
}

/** Canonical pretty JSON used only by the committed HIR/ZIR goldens. */
export function serializeHir(value: unknown): string {
  return JSON.stringify(sortDeep(value), (_key, v) => typeof v === 'bigint' ? v.toString() : v, 1) + '\n';
}

function sortDeep(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(sortDeep);
  if (value && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const key of Object.keys(value as Record<string, unknown>).sort()) {
      const item = (value as Record<string, unknown>)[key];
      if (item !== undefined) out[key] = sortDeep(item);
    }
    return out;
  }
  return value;
}
