// catalog.test.ts — the FORM-E catalog must match spec/form/language-semantics.md
// §7 EXACTLY, in both directions (plan W3.2: "a test must assert the code list
// MATCHES the spec exactly"). Codes are frozen once W3.2 ships.

import { test } from 'node:test';
import assert from 'node:assert/strict';

import { FORM_E_CATALOG, catalogCodes, extractSpecCodes } from '../../src/frontend/diagnostics.js';
import { NEGATIVE_CORPUS, EXEMPT_CODES } from './negative_corpus.js';
import { specForm } from './helpers.js';

const specCodes = extractSpecCodes(specForm('language-semantics.md'));

test('catalog: spec §7 tables parse to a non-empty code set', () => {
  assert.ok(specCodes.length >= 100, `spec yielded only ${specCodes.length} codes`);
});

test('catalog: compiler set === spec set (both directions)', () => {
  const ours = catalogCodes();
  assert.deepEqual(ours, specCodes, 'compiler catalog must equal the spec §7 code list');
});

test('catalog: the spec defines exactly 136 codes', () => {
  // Pins the frozen size; a change here is a spec event, not a compiler tweak.
  assert.equal(specCodes.length, 136);
});

test('catalog: negative corpus covers every frontend-raisable code', () => {
  const covered = new Set(NEGATIVE_CORPUS.map((c) => c.code));
  covered.add('FORM-E-832'); // exercised by the generated case in corpus.test.ts
  const missing = catalogCodes().filter((c) => !covered.has(c) && !EXEMPT_CODES.includes(c));
  assert.deepEqual(missing, [], 'every non-exempt code needs at least one corpus case');
  // exempt codes must genuinely be absent from the corpus
  for (const ex of EXEMPT_CODES) assert.ok(!covered.has(ex), `${ex} is exempt, not raisable`);
});

test('catalog: corpus only uses codes the spec defines', () => {
  for (const c of NEGATIVE_CORPUS) {
    assert.ok(FORM_E_CATALOG.has(c.code), `corpus case ${c.name} uses unknown code ${c.code}`);
  }
});

test('catalog: exempt codes are exactly the documented non-frontend codes', () => {
  assert.deepEqual(
    [...EXEMPT_CODES].sort(),
    ['FORM-E-668', 'FORM-E-821', 'FORM-E-822', 'FORM-E-830', 'FORM-E-831'],
  );
});
