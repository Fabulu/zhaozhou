// index.ts — the Form frontend public surface (W3.2).
//
// compileFrontend: UTF-8 sources -> ASTs + diagnostics + the compile-time
// schedule (D6 analysis feeding W3.3). Diagnostics are collected, never
// thrown (D2). NO PARTIAL EMISSION: when `ok` is false the caller must write
// nothing (.zprog/C++/zmap/zcost) — the backend checks `ok` first.

import { ModuleAst, serializeAst } from './ast.js';
import { Diagnostic, DiagnosticSink } from './diagnostics.js';
import { parseSource } from './parser.js';
import { CheckResult, checkModules } from './checker.js';

export * from './span.js';
export * from './diagnostics.js';
export * from './lexer.js';
export * from './ast.js';
export * from './parser.js';
export * from './checker.js';

export interface FrontendResult {
  /** true iff zero error-severity diagnostics */
  ok: boolean;
  modules: ModuleAst[];
  diagnostics: Diagnostic[];
  check: CheckResult | null;
}

export function compileFrontend(sources: Record<string, string | Uint8Array>): FrontendResult {
  const sink = new DiagnosticSink();
  const modules: ModuleAst[] = [];
  for (const [file, source] of Object.entries(sources)) {
    modules.push(parseSource(source, file, sink));
  }
  const check = sink.hasErrors ? null : checkModules(modules, sink);
  return {
    ok: !sink.hasErrors,
    modules,
    diagnostics: sink.diagnostics,
    check,
  };
}

export { serializeAst };
