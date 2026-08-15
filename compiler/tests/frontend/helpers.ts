// helpers.ts — shared frontend-test utilities (W3.2). Repo root is located
// from compiled test output (dist/tests/frontend/), same discipline as the
// wave-1/2 suites.

import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { compileFrontend, Diagnostic, FrontendResult } from '../../src/frontend/index.js';

export function repoRoot(): string {
  let dir = path.dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i++) {
    if (existsSync(path.join(dir, 'spec', 'form', 'language-semantics.md'))) return dir;
    dir = path.dirname(dir);
  }
  throw new Error('repo root not found (spec/form/language-semantics.md missing?)');
}

export function corpusDir(): string {
  return path.join(repoRoot(), 'compiler', 'tests', 'frontend', 'corpus');
}

export function specForm(name: string): string {
  return readFileSync(path.join(repoRoot(), 'spec', 'form', name), 'utf8');
}

/** Compile one source (or module set) and return the result + error codes. */
export function compile(src: string | Record<string, string> | Uint8Array): FrontendResult & { codes: string[] } {
  const sources: Record<string, string | Uint8Array> =
    typeof src === 'string' ? { 'm.form': src }
    : src instanceof Uint8Array ? { 'm.form': src }
    : src;
  const r = compileFrontend(sources);
  return { ...r, codes: errorCodes(r.diagnostics) };
}

export function errorCodes(diags: Diagnostic[]): string[] {
  const seen: string[] = [];
  for (const d of diags) {
    if (d.severity === 'error' && !seen.includes(d.code)) seen.push(d.code);
  }
  return seen;
}

export function firstError(r: FrontendResult): Diagnostic | undefined {
  return r.diagnostics.find((d) => d.severity === 'error');
}
