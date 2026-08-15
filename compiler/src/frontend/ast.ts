// ast.ts — Form L1 AST (W3.2). Every node carries a byte-offset SourceSpan
// (D2; language-semantics §2). The canonical serializer produces
// byte-stable output (sorted keys, LF, no timestamps) for the committed AST
// goldens: same input -> identical bytes, across runs and machines.

import { SourceSpan } from './span.js';

export interface Node { readonly span: SourceSpan }

// ---------------------------------------------------------------------------
// Module + declarations
// ---------------------------------------------------------------------------

export interface ModuleAst extends Node {
  kind: 'Module';
  name: string;
  imports: ImportDecl[];
  decls: TopDecl[];
}

export interface ImportDecl extends Node {
  kind: 'Import';
  module: string;
  /** empty = whole-module import (`import m;` -> qualified `m.name` use) */
  names: string[];
}

export type TopDecl =
  | ConstDecl | EnumDecl | StructDecl | PoolDecl | GlobalDecl | FnDecl
  | SystemDecl | FieldDecl | PresentationDecl | ScenarioDecl | SoundDecl
  | BadDecl;

export interface ConstDecl extends Node {
  kind: 'Const';
  name: string;
  type: TypeExpr;
  init: Expr;
}

export interface EnumDecl extends Node {
  kind: 'Enum';
  name: string;
  members: EnumMember[];
}
export interface EnumMember extends Node { name: string; value: bigint | null }

export interface StructDecl extends Node {
  kind: 'Struct';
  name: string;
  fields: StructField[];
}
export interface StructField extends Node { name: string; type: TypeExpr }

export interface PoolDecl extends Node {
  kind: 'Pool';
  name: string;
  structName: string;
  /** capacity literal (int token) or const name; null when omitted */
  capacity: CapacityExpr;
}
export type CapacityExpr = { kind: 'int'; value: bigint; span: SourceSpan } | { kind: 'name'; name: string; span: SourceSpan } | null;

export interface GlobalDecl extends Node {
  kind: 'Global';
  name: string;
  type: TypeExpr;
  init: Expr;
}

export interface FnDecl extends Node {
  kind: 'Fn';
  name: string;
  params: Param[];
  ret: TypeExpr;
  body: Stmt[];
}
export interface Param extends Node { name: string; type: TypeExpr }

export interface SystemDecl extends Node {
  kind: 'System';
  name: string;
  every: bigint;
  /** explicit stagger rate when authored (`stagger 4 over p`); else every-rate */
  staggerRate: bigint | null;
  staggerPool: string | null;
  staggerSpan: SourceSpan | null;
  reads: Access[];
  writes: Access[];
  body: Stmt[];
}

/** A `reads`/`writes` entry: dotted component path or bare name. */
export interface Access extends Node { kind: 'access'; parts: string[] }

export interface FieldDecl extends Node {
  kind: 'Field';
  /** as authored after '@' — 'earth' | 'flow' admitted; anything else refused */
  profile: string;
  profileSpan: SourceSpan;
  name: string;
  /** params struct name (params: P); null when absent */
  paramsStruct: string | null;
  paramsSpan: SourceSpan | null;
  /** as authored after '->' */
  returns: string;
  footprint: Footprint;
  maxOps: bigint;
  body: FieldStmt[];
}

export type Footprint =
  | { kind: 'none'; span: SourceSpan }
  | { kind: 'rect'; args: Expr[]; span: SourceSpan }
  | { kind: 'circle'; args: Expr[]; span: SourceSpan }
  | { kind: 'capsule'; args: Expr[]; span: SourceSpan };

export type FieldStmt =
  | { kind: 'field_let'; name: string; value: FieldExpr; span: SourceSpan }
  | { kind: 'field_return'; record: string; fields: RecordField[]; span: SourceSpan };

// The field-dialect expression surface is the general Expr subset; admission
// is checked by the checker (language-semantics §6.3), so the dialect reuses
// Expr nodes. FieldExpr aliases it for readability.
export type FieldExpr = Expr;

export interface PresentationDecl extends Node {
  kind: 'Presentation';
  name: string;
  items: PresentationItem[];
}
export type PresentationItem = ViewItem | SharedBudgetItem | EmitStmt | Stmt | BadStmt;

export interface ViewItem extends Node {
  kind: 'view';
  id: bigint;
  /** null when `from` absent (checker raises FORM-E-607) */
  camera: Expr | null;
  budgetPct: bigint;
}

export interface SharedBudgetItem extends Node { kind: 'shared_budget'; pct: bigint }

export interface EmitStmt extends Node {
  kind: 'emit';
  /** as authored */
  emitKind: string;
  args: EmitArg[];
}
export interface EmitArg extends Node { kind: 'emit_arg'; name: string; value: Expr }

export interface ScenarioDecl extends Node {
  kind: 'Scenario';
  name: string;
  items: ScenarioItem[];
}
export type ScenarioItem =
  | { kind: 'seed'; value: bigint; span: SourceSpan }
  | { kind: 'load'; target: string; span: SourceSpan }
  | { kind: 'spawn_player'; index: bigint; at: string; span: SourceSpan }
  | { kind: 'at'; tick: bigint; action: Expr; span: SourceSpan }
  | { kind: 'assert'; expr: Expr; tolerance: Expr | null; span: SourceSpan }
  | { kind: 'capture'; frame: bigint; name: string; span: SourceSpan }
  | { kind: 'assert_budget'; budgetSet: string; span: SourceSpan };

export interface SoundDecl extends Node {
  kind: 'Sound';
  name: string;
  sample: string;
  sampleSpan: SourceSpan;
  params: { kind: 'gain' | 'pitch' | 'pan'; value: Expr; span: SourceSpan }[];
}

/** Placeholder for unparseable declarations (recovery keeps spans). */
export interface BadDecl extends Node { kind: 'BadDecl'; text: string }

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type TypeExpr =
  | { kind: 'named'; name: string; span: SourceSpan }
  | { kind: 'array'; elem: TypeExpr; len: { kind: 'int'; value: bigint } | { kind: 'name'; name: string }; span: SourceSpan };

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

export type Stmt =
  | LetStmt | AssignStmt | IfStmt | ForStmt | CallStmt
  | SpawnStmt | KillStmt | ReturnStmt | ApplyStmt | BadStmt;

export interface LetStmt extends Node {
  kind: 'let';
  name: string;
  type: TypeExpr | null;
  init: Expr;
}

export interface AssignStmt extends Node {
  kind: 'assign';
  target: Expr;
  value: Expr;
}

export interface IfStmt extends Node {
  kind: 'if';
  cond: Expr;
  then: Stmt[];
  else: Stmt[] | Stmt | null;
}

export interface ForStmt extends Node {
  kind: 'for';
  varName: string;
  /** explicit a..b range or pool sugar */
  range: { kind: 'range'; lo: Expr; hi: Expr } | { kind: 'pool'; pool: string; poolSpan: SourceSpan };
  body: Stmt[];
}

export interface CallStmt extends Node {
  kind: 'call_stmt';
  call: Expr;
}

export interface SpawnStmt extends Node {
  kind: 'spawn';
  pool: string;
  value: RecordLit;
}

export interface KillStmt extends Node {
  kind: 'kill';
  pool: string;
  index: Expr;
}

export interface ReturnStmt extends Node {
  kind: 'return';
  value: Expr | null;
}

/** `apply terrain_field name(args) duration expr;` — sim statement (§6.4). */
export interface ApplyStmt extends Node {
  kind: 'apply';
  applyKind: string; // 'terrain_field' | 'flow'
  program: string;
  programSpan: SourceSpan;
  args: EmitArg[];
  duration: Expr;
}

export interface BadStmt extends Node { kind: 'bad_stmt'; text: string }

// ---------------------------------------------------------------------------
// Expressions (Pratt tree)
// ---------------------------------------------------------------------------

export type Expr =
  | LiteralExpr | IdentExpr | MemberExpr | IndexExpr | CallExpr
  | UnaryExpr | BinaryExpr | IfExpr | RecordLit | RangeExpr | StringExpr;

export interface LiteralExpr extends Node {
  kind: 'literal';
  lit: 'int' | 'frac' | 'tick' | 'colour' | 'bool';
  /** verbatim source text */
  text: string;
  intVal?: bigint;
  frac?: { intDigits: string; fracDigits: string; suffix: string };
}

export interface IdentExpr extends Node { kind: 'ident'; name: string }

export interface MemberExpr extends Node { kind: 'member'; obj: Expr; field: string; fieldSpan: SourceSpan }

export interface IndexExpr extends Node { kind: 'index'; obj: Expr; index: Expr }

export interface CallExpr extends Node { kind: 'call'; callee: Expr; args: Expr[] }

export interface UnaryExpr extends Node { kind: 'unary'; op: '-' | '!' | '~'; operand: Expr }

export interface BinaryExpr extends Node { kind: 'binary'; op: string; l: Expr; r: Expr }

/** if select-expression: both arms always evaluated (§2.2). */
export interface IfExpr extends Node {
  kind: 'if_expr';
  cond: Expr;
  then: Expr;
  else: Expr;
}

export interface RecordLit extends Node {
  kind: 'record';
  typeName: string;
  fields: RecordField[];
}
export interface RecordField extends Node { name: string; value: Expr }

export interface RangeExpr extends Node { kind: 'range'; lo: Expr; hi: Expr }

export interface StringExpr extends Node { kind: 'string'; value: string }

// ---------------------------------------------------------------------------
// Canonical serialization (AST goldens; byte-stable by construction)
// ---------------------------------------------------------------------------

function canonical(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') {
    if (value instanceof Map || value instanceof Set) {
      throw new Error('canonical AST must not contain Map/Set');
    }
    const out: Record<string, unknown> = {};
    for (const key of Object.keys(value as Record<string, unknown>).sort()) {
      const v = (value as Record<string, unknown>)[key];
      if (v === undefined) continue; // prune optionals
      out[key] = canonical(v);
    }
    return out;
  }
  return value;
}

/** Byte-stable JSON of any AST node: sorted keys, bigint as string, LF, trailing newline. */
export function serializeAst(node: Node): string {
  return (
    JSON.stringify(canonical(node), (_k, v) => (typeof v === 'bigint' ? v.toString() : v), 1) + '\n'
  );
}
