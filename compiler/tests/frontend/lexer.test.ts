// lexer.test.ts — lexical law (language-semantics §1): byte-offset spans,
// CRLF tolerance, nesting block comments, target-typed literal FORMS (the
// checker owns target-typing; these tests pin token shapes), keyword set,
// punctuation, and the UTF-8 gate.

import { test } from 'node:test';
import assert from 'node:assert/strict';

import { tokenize } from '../../src/frontend/lexer.js';
import { DiagnosticSink } from '../../src/frontend/diagnostics.js';

function lex(src: string | Uint8Array, file = 'm.form') {
  const sink = new DiagnosticSink();
  const bytes = typeof src === 'string' ? new TextEncoder().encode(src) : src;
  return { tokens: tokenize(bytes, file, sink), sink, bytes };
}

test('lexer: byte offsets over CRLF and multi-byte UTF-8 (CR and LF count, neither in tokens)', () => {
  // "module\r\nm {\r\n}" — the \r\n is two bytes of offset, no token covers it
  const { tokens } = lex('module\r\nm {\r\n}\r\n');
  const kw = tokens[0]!;
  const id = tokens[1]!;
  assert.equal(kw.text, 'module');
  assert.equal(kw.span.start, 0);
  assert.equal(kw.span.end, 6);
  assert.equal(id.text, 'm');
  assert.equal(id.span.start, 8); // "module"(6) + CRLF(2 bytes) -> 'm' at byte 8
});

test('lexer: non-ASCII comment content shifts byte offsets (UTF-8 multibyte)', () => {
  const { tokens } = lex('// é\nconst A: u32 = 1;');
  const konst = tokens.find((t) => t.text === 'const')!;
  // 'é' is 2 bytes: '// ' (3) + é (2) + \n (1) = 6 bytes before 'const'
  assert.equal(konst.span.start, 6);
});

test('lexer: nesting block comments', () => {
  const { sink, tokens } = lex('/* a /* b */ c */ const A: u32 = 1;');
  assert.equal(sink.diagnostics.length, 0);
  assert.equal(tokens[0]!.text, 'const');
});

test('lexer: literal token shapes (§1.2 table)', () => {
  const { tokens } = lex('1.5 1.5m 0.65px 2.25w 0.25turn 90deg 45% 120t 0x5A17 #3060A0 #803060A0 true');
  const [a, b, c, d, e, f, g, h, i, j, k, l] = tokens as any[];
  assert.equal(a.frac.suffix, '');
  assert.equal(b.frac.suffix, 'm');
  assert.equal(c.frac.suffix, 'px');
  assert.equal(d.frac.suffix, 'w');
  assert.equal(e.frac.suffix, 'turn');
  assert.equal(f.frac.suffix, 'deg');
  assert.deepEqual(g.frac, { intDigits: '45', fracDigits: '', suffix: '%' });
  assert.equal(h.kind, 'tick');
  assert.equal(h.intVal, 120n);
  assert.equal(i.intVal, 0x5a17n);
  assert.equal(j.kind, 'colour');
  assert.equal(j.intVal, 0x3060a0n);
  assert.equal(k.intVal, 0x803060a0n);
  assert.equal(l.kind, 'kw');
  assert.equal(l.text, 'true');
});

test('lexer: tick suffix boundary — 120ticks is int + keyword, not a tick literal', () => {
  const { tokens } = lex('120ticks');
  assert.equal(tokens[0]!.kind, 'int');
  assert.equal(tokens[1]!.kind, 'kw');
});

test('lexer: punctuation two-char first', () => {
  const { tokens } = lex('a :: b -> c .. d == e != f <= g >= h && i || j << k >> l');
  const texts = tokens.map((t) => t.text);
  for (const p of ['::', '->', '..', '==', '!=', '<=', '>=', '&&', '||', '<<', '>>']) {
    assert.ok(texts.includes(p), p);
  }
});

test('lexer: domain keywords are keywords (OUT-list refusals, §1.1)', () => {
  const { tokens } = lex('build warp formation stamp macro class extern f32 string while break');
  for (const t of tokens.slice(0, -1)) assert.equal(t.kind, 'kw', t.text); // drop <eof>
});

test('lexer: lane-name soft keywords lex as keywords', () => {
  const { tokens } = lex('sample material nav_cost height velocity age phase dt attr');
  for (const t of tokens.slice(0, -1)) assert.equal(t.kind, 'kw', t.text); // drop <eof>
});

test('lexer: string escapes — only \\\\ and \\"', () => {
  const ok = lex('x = "a\\"b\\\\c";');
  const str = ok.tokens.find((t) => t.kind === 'string')!;
  assert.equal(str.text, 'a"b\\c');
  const bad = lex('x = "a\\nb";');
  assert.equal(bad.sink.errorCodes().length, 1);
  assert.equal(bad.sink.errorCodes()[0], 'FORM-E-004');
});

test('lexer: invalid UTF-8 is FORM-E-001 and stops the pipeline', () => {
  const bytes = new TextEncoder().encode('module m { // ok\n}\n');
  bytes[bytes.length - 2] = 0xff; // clobber the final '}'
  const { sink } = lex(bytes);
  assert.deepEqual(sink.errorCodes(), ['FORM-E-001']);
});
