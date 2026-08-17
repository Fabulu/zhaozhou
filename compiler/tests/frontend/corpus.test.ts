// corpus.test.ts — the committed corpora (plan W3.2 acceptance):
//   * positive: every IN feature, compiled as ONE multi-module cartridge;
//     AST goldens committed byte-stable (same input -> identical serialized
//     AST; run-twice equality asserted live, golden equality vs disk).
//   * negative: every frontend-raisable FORM-E code, first-diagnostic match.
// Regenerate goldens with UPDATE_GOLDENS=1 npm run -w compiler test.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, readFileSync, readdirSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';

import { DiagnosticSink, parseSource, serializeAst } from '../../src/frontend/index.js';
import { checkModules } from '../../src/frontend/checker.js';
import { compile, corpusDir } from './helpers.js';
import { NEGATIVE_CORPUS } from './negative_corpus.js';

function loadPositive(): Record<string, string> {
  const dir = path.join(corpusDir(), 'positive');
  const sources: Record<string, string> = {};
  for (const f of readdirSync(dir).sort()) {
    if (f.endsWith('.form')) sources[f] = readFileSync(path.join(dir, f), 'utf8');
  }
  return sources;
}

test('positive corpus: the whole cartridge compiles with zero errors', () => {
  const sources = loadPositive();
  assert.ok(Object.keys(sources).length >= 6, 'expected at least six corpus modules');
  const r = compile(sources);
  assert.deepEqual(r.codes, [], r.diagnostics.map((d) => `${d.code} ${d.file}:${d.span.start} ${d.message}`).join('\n'));
  assert.ok(r.ok);
  // the schedule exists and is non-empty (D6 analysis feeding W3.3)
  assert.ok(r.check!.schedule!.phases.length >= 3);
});

test('positive corpus: AST goldens are byte-stable (run twice, identical bytes)', () => {
  const sources = loadPositive();
  const parse1 = parseAll(sources);
  const parse2 = parseAll(sources);
  for (const file of Object.keys(sources)) {
    const a = serializeAst(parse1[file]!);
    const b = serializeAst(parse2[file]!);
    assert.equal(a, b, `${file} AST serialization must be deterministic`);
  }
});

test('positive corpus: committed goldens match (UPDATE_GOLDENS=1 to regenerate)', () => {
  const sources = loadPositive();
  const asts = parseAll(sources);
  const goldenDir = path.join(corpusDir(), 'golden');
  if (process.env.UPDATE_GOLDENS) mkdirSync(goldenDir, { recursive: true });
  for (const file of Object.keys(sources).sort()) {
    const goldenPath = path.join(goldenDir, `${file}.ast.json`);
    const serialized = serializeAst(asts[file]!);
    if (process.env.UPDATE_GOLDENS) {
      writeFileSync(goldenPath, serialized, 'utf8');
      continue;
    }
    assert.ok(existsSync(goldenPath), `${goldenPath} missing — run with UPDATE_GOLDENS=1`);
    const golden = readFileSync(goldenPath, 'utf8');
    assert.equal(serialized, golden, `${file} AST drifted from its committed golden`);
  }
});

function parseAll(sources: Record<string, string>) {
  const out: Record<string, ReturnType<typeof parseSource>> = {};
  for (const [file, src] of Object.entries(sources)) {
    out[file] = parseSource(src, file, new DiagnosticSink());
  }
  return out;
}

test('positive corpus: goldens parse cleanly before serialization (no lexer/parser diagnostics)', () => {
  const sources = loadPositive();
  for (const [file, src] of Object.entries(sources)) {
    const sink = new DiagnosticSink();
    parseSource(src, file, sink);
    assert.deepEqual(sink.diagnostics, [], `${file} must lex+parse clean`);
  }
});

test('positive corpus: checked modules carry a schedule with the corpus systems', () => {
  const sources = loadPositive();
  const sink = new DiagnosticSink();
  const modules = Object.entries(sources).map(([f, s]) => parseSource(s, f, sink));
  const parseDiags = [...sink.diagnostics];
  assert.deepEqual(parseDiags, []);
  const { schedule } = checkModules(modules, sink);
  const checkerDiags = [...sink.diagnostics].map((d) => `${d.code} ${d.message}`).join('\n');
  assert.deepEqual([...sink.diagnostics], [], checkerDiags);
  const names = schedule!.phases.flatMap((p) => p.systems.map((s) => s.name));
  for (const expected of ['input_latch', 'spawn_waves', 'integrate', 'reap', 'shatter_tick', 'apply_earth']) {
    assert.ok(names.includes(expected), `schedule must contain ${expected}`);
  }
});

// ---------------------------------------------------------------------------
// The negative corpus: every refusal, exact code
// ---------------------------------------------------------------------------

for (const c of NEGATIVE_CORPUS) {
  test(`negative: ${c.code} (${c.name})`, () => {
    const src = typeof c.src === 'function' ? c.src() : c.src;
    const r = compile(src);
    assert.ok(!r.ok, 'case must not compile');
    const first = r.diagnostics.find((d) => d.severity === 'error');
    assert.ok(first, 'at least one error diagnostic');
    assert.equal(first!.code, c.code,
      `expected first error ${c.code}, got ${first!.code} (${first!.message}); all: ${r.codes.join(',')}`);
  });
}

test('source-ID registry ignores declarations that allocate no source row', () => {
  const decls = Array.from({ length: 257 }, (_, i) => `const K${i}: u32 = ${i % 97};`).join('\n');
  const r = compile(`module many_constants {\n${decls}\n}\n`);
  assert.ok(!r.codes.includes('FORM-E-832'));
});
