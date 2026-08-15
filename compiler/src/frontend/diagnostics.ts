// diagnostics.ts — structured diagnostics + the FORM-E catalog (W3.2, D2).
//
// Law: spec/form/language-semantics.md §7 — diagnostics are
// `{file, span, code: "FORM-E-nnn", message}`, collected never thrown;
// non-zero exit on any error; no partial emission. Warnings (FORM-W-nnn)
// never affect semantics. One code per rule; codes are frozen once W3.2
// ships. The catalog below mirrors the spec tables verbatim (code -> rule
// summary); `frontend_catalog.test.ts` asserts the code SET matches the
// spec's §7 tables exactly in both directions.

import { SourceSpan } from './span.js';

export type Severity = 'error' | 'warning';

export interface Diagnostic {
  readonly file: string;
  readonly span: SourceSpan;
  readonly code: string; // "FORM-E-nnn" | "FORM-W-nnn"
  readonly message: string;
  readonly severity: Severity;
}

/** Collects diagnostics; never throws (D2). Errors are fatal, warnings are not. */
export class DiagnosticSink {
  readonly diagnostics: Diagnostic[] = [];

  error(code: string, span: SourceSpan, message: string): void {
    this.diagnostics.push({ file: span.file, span, code, message, severity: 'error' });
  }

  warning(code: string, span: SourceSpan, message: string): void {
    this.diagnostics.push({ file: span.file, span, code, message, severity: 'warning' });
  }

  get errors(): Diagnostic[] {
    return this.diagnostics.filter((d) => d.severity === 'error');
  }

  get hasErrors(): boolean {
    return this.diagnostics.some((d) => d.severity === 'error');
  }

  /** Unique error codes in first-seen order (test helper). */
  errorCodes(): string[] {
    const seen: string[] = [];
    for (const d of this.errors) if (!seen.includes(d.code)) seen.push(d.code);
    return seen;
  }
}

// ---------------------------------------------------------------------------
// FORM-E catalog (spec/form/language-semantics.md §7, mirrored 1:1).
// Frontend-raisable codes are all of these except the explicitly marked
// runtime/pack-time rows (E-668 table grammar, E-821/822 runtime aborts,
// E-830/831 pack-time resolution) — see the catalog test for the exceptions
// list, each backed by a spec-issue note in the W3.2 report.
// ---------------------------------------------------------------------------

export const FORM_E_CATALOG: ReadonlyMap<string, string> = new Map<string, string>([
  // FORM-E-001..009 — lexical
  ['FORM-E-001', 'source file is not valid UTF-8'],
  ['FORM-E-002', 'unterminated block comment'],
  ['FORM-E-003', 'unterminated string literal'],
  ['FORM-E-004', 'illegal escape in string (only \\\\ and \\")'],
  ['FORM-E-005', 'character does not begin any token'],
  ['FORM-E-006', 'malformed integer literal (0x without digits, leading-zero octal, 0X, bad colour)'],
  ['FORM-E-007', 'integer/literal exceeds its target type range'],
  ['FORM-E-008', 'fractional literal not exactly representable in its Q format'],
  ['FORM-E-009', 'literal exceeds byte-length limit (ident 64, string 256)'],
  // FORM-E-100..110 — parse
  ['FORM-E-100', 'expected token, found other'],
  ['FORM-E-101', 'unexpected token at top level (declaration keyword required)'],
  ['FORM-E-102', 'duplicate declaration name in module (params/fields/members)'],
  ['FORM-E-103', 'module not first token of file, or trailing content after module'],
  ['FORM-E-104', 'record literal names a field the type does not have'],
  ['FORM-E-105', 'record literal omits a field'],
  ['FORM-E-106', 'duplicate field in record literal'],
  ['FORM-E-107', '`..` range used outside a `for` header'],
  ['FORM-E-108', 'statement form not admitted in this context'],
  ['FORM-E-109', 'keyword used as identifier'],
  ['FORM-E-110', 'expression grammar violation not otherwise classified'],
  // FORM-E-200..229 — modules, names, imports
  ['FORM-E-201', 'duplicate top-level declaration in a module'],
  ['FORM-E-202', 'import of unknown module'],
  ['FORM-E-203', 'use of an unknown name (unresolved identifier)'],
  ['FORM-E-204', 'import cycle'],
  ['FORM-E-205', 'ambiguous unqualified name (two imports bring the same name)'],
  ['FORM-E-206', 'use of a private name from another module without qualification'],
  // FORM-E-300..339 — types
  ['FORM-E-300', 'type mismatch'],
  ['FORM-E-301', 'unknown type name'],
  ['FORM-E-302', 'let re-assignment (single-assignment law)'],
  ['FORM-E-303', 'use of a local before its let'],
  ['FORM-E-304', 'wrong argument count in call'],
  ['FORM-E-305', 'array index not u32/i32'],
  ['FORM-E-306', 'struct field access on non-struct'],
  ['FORM-E-307', 'enum member does not exist'],
  ['FORM-E-308', 'non-constant initializer where a constant expression is required'],
  ['FORM-E-309', 'fn return type mismatch with returned expression'],
  ['FORM-E-310', 'missing return in a value-returning fn'],
  ['FORM-E-311', 'if select-expression branches have different types'],
  ['FORM-E-312', 'bool expected (condition position)'],
  ['FORM-E-313', 'wrong literal suffix for target type'],
  ['FORM-E-320', 'negative literal to u32'],
  ['FORM-E-330', 'world3/velocity3 mixed in one operator (space-typing)'],
  ['FORM-E-331', 'mixed-precision binary operands (fx16 vs fx24)'],
  ['FORM-E-332', 'fx24/world/velocity used inside a field declaration (Q2)'],
  ['FORM-E-333', 'assignment target is not an lvalue / not declared writable'],
  ['FORM-E-334', 'conversion intrinsic argument type wrong'],
  // FORM-E-400..459 — domains and effects
  ['FORM-E-400', 'write to truth state outside a system (in fn/presentation)'],
  ['FORM-E-401', 'pool/global written by a system that did not declare it in writes'],
  ['FORM-E-402', 'pool/global read by a block that did not declare it in reads'],
  ['FORM-E-403', 'input read outside sim/scenario'],
  ['FORM-E-404', 'random stream in field dialect'],
  ['FORM-E-405', 'presentation block calls a mutating intrinsic (spawn/kill/apply/assign)'],
  ['FORM-E-406', 'spawn/kill outside sim'],
  ['FORM-E-407', 'system reads and writes the same component where separation is required'],
  ['FORM-E-408', 'sound referenced before declaration'],
  ['FORM-E-409', 'scenario statement outside scenario block'],
  // FORM-E-460..479 — terrain/present object model
  ['FORM-E-460', 'direct terrain truth mutation outside an @earth application'],
  ['FORM-E-461', 'apply terrain_field outside a sim system'],
  ['FORM-E-462', 'applied field program is not @earth'],
  ['FORM-E-463', 'duration of zero or missing footprint arguments'],
  ['FORM-E-464', 'camera binding expression not a world3 transform source'],
  // FORM-E-500..519 — scheduling (deterministic-scheduling.md)
  ['FORM-E-500', 'two systems write one state component in one phase (both spans cited)'],
  ['FORM-E-501', 'descending or provably-empty for range'],
  ['FORM-E-502', 'for trip count not statically bounded'],
  ['FORM-E-503', 'pool mutation (spawn/kill) inside pool iteration where refused'],
  ['FORM-E-504', 'stagger without exactly one iteration pool'],
  ['FORM-E-505', 'cyclic read-write dependency between systems'],
  ['FORM-E-506', 'every N with N = 0'],
  ['FORM-E-507', 'stagger rate N not equal to the system every N'],
  // FORM-E-600..619 — presentation emit
  ['FORM-E-600', 'emit statement outside a presentation block'],
  ['FORM-E-601', 'required emit argument missing'],
  ['FORM-E-602', 'unknown emit argument name'],
  ['FORM-E-603', 'unknown emit kind'],
  ['FORM-E-604', 'more than two views declared'],
  ['FORM-E-605', 'view budgets + shared sum exceeds 100%'],
  ['FORM-E-606', 'view id repeated or not 0/1'],
  ['FORM-E-607', 'view has no camera binding'],
  ['FORM-E-608', 'draw_population names a non-pool'],
  ['FORM-E-609', 'audio names a non-sound'],
  ['FORM-E-610', 'resource page-id argument is not a u32 const'],
  // FORM-E-650..679 — field dialect
  ['FORM-E-650', '@<profile> other than earth/flow on a field declaration'],
  ['FORM-E-651', 'if statement, loop, or early return in field body (branchless law)'],
  ['FORM-E-652', 'call (even pure fn) inside field body'],
  ['FORM-E-653', 'statement other than let/return in field body'],
  ['FORM-E-654', 'declared max_ops above the profile ceiling (earth 32, flow 48)'],
  ['FORM-E-655', 'lowered instruction count above the declared max_ops'],
  ['FORM-E-656', 'state access (pool/global/terrain) inside field body'],
  ['FORM-E-657', 'input/random inside field body'],
  ['FORM-E-658', 'non-fx16 lane/local type inside field body'],
  ['FORM-E-659', 'more than 64 live values (register budget)'],
  ['FORM-E-660', 'params struct field not fx16'],
  ['FORM-E-661', 'params struct exceeds the profile p-lane count (8 earth / 4 flow)'],
  ['FORM-E-662', 'params binding missing or not a struct'],
  ['FORM-E-663', 'return record does not match the profile output record'],
  ['FORM-E-664', 'attached pool struct does not match the flow lane mapping'],
  ['FORM-E-665', 'expression form not admitted in the field dialect'],
  ['FORM-E-666', 'footprint/none mismatch for the profile'],
  ['FORM-E-667', 'flow program applied to a pool other than its mapping'],
  ['FORM-E-668', 'table declaration malformed or tables exceed table-byte budget'],
  // FORM-E-700..729 — L1 OUT list (D1; refused, never silently parsed)
  ['FORM-E-700', 'form declarations / representation ladders (L3)'],
  ['FORM-E-701', 'macro declarations'],
  ['FORM-E-702', 'generics beyond pool capacity literals'],
  ['FORM-E-703', 'class/interface/inheritance (OOP)'],
  ['FORM-E-704', 'closures / lambda syntax'],
  ['FORM-E-705', 'string type / string operations'],
  ['FORM-E-706', 'first-class functions / function values'],
  ['FORM-E-707', 'pointers / references'],
  ['FORM-E-708', 'while / unbounded loops'],
  ['FORM-E-709', 'recursion (self or mutual; call graph)'],
  ['FORM-E-710', 'host FFI (extern/importc/escape hatches)'],
  ['FORM-E-711', 'floating-point types/literals (f32/f64/float/double/scientific)'],
  ['FORM-E-712', 'break/continue'],
  ['FORM-E-713', 'terrain_material declarations (L4)'],
  ['FORM-E-714', 'population declarations (L3)'],
  ['FORM-E-715', '@build/@warp/@formation/@stamp domains (L2 profiles)'],
  ['FORM-E-716', 'surface/spell declarations (L2+)'],
  ['FORM-E-717', 'layer/after/transparent_group/barrier ordering constructs (L3)'],
  ['FORM-E-718', 'audio graph constructs beyond the tone-event declaration (L4)'],
  ['FORM-E-719', 'dynamic allocation / unbounded collection growth'],
  ['FORM-E-720', 'assert_budget naming an undeclared budget set (L3 registry)'],
  // FORM-E-800..839 — capacities, bounds, packing
  ['FORM-E-800', 'pool capacity not a positive integer or u32 const'],
  ['FORM-E-801', 'pool element type is not a struct / is recursive'],
  ['FORM-E-820', 'compile-time-provable array/pool index out of bounds'],
  ['FORM-E-821', 'pool overflow at spawn (deterministic runtime abort)'],
  ['FORM-E-822', 'pool/array index out of bounds at runtime (deterministic abort)'],
  ['FORM-E-830', 'cartridge page-id const unresolved at pack time'],
  ['FORM-E-831', 'cartridge kind mismatch for a page id'],
  ['FORM-E-832', 'source-ID registry overflow (> 65536 declarations per module)'],
  // FORM-E-900..919 — scenarios
  ['FORM-E-900', 'seed missing or repeated in a scenario'],
  ['FORM-E-901', 'load target not a module/map known to the cartridge'],
  ['FORM-E-902', 'spawn player index not 0..3'],
  ['FORM-E-903', 'at N ticks not ascending across the scenario script'],
  ['FORM-E-904', 'scenario action names an unknown system/spell entry'],
  ['FORM-E-905', 'capture frame N before any earlier at tick'],
  ['FORM-E-906', 'assertion references undeclared state'],
  ['FORM-E-907', 'tolerance literal not representable (fx16 exactness law)'],
]);

/** All catalog codes, sorted. */
export function catalogCodes(): string[] {
  return [...FORM_E_CATALOG.keys()].sort();
}

/**
 * Extract the FORM-E code set from the spec markdown (§7 tables). Used by the
 * catalog test to prove the compiler's catalog matches the spec exactly.
 * Table rows look like `| FORM-E-001 | rule text |`.
 */
export function extractSpecCodes(specMarkdown: string): string[] {
  const codes = new Set<string>();
  for (const m of specMarkdown.matchAll(/^\|\s*(FORM-E-\d+)\s*\|/gm)) codes.add(m[1]!);
  return [...codes].sort();
}
