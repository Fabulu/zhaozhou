// field_fuzz_corpus.test.ts — generates + commits the bounded-random Field
// IR corpus (plan W5 deliverable 8: tests/fuzz/field_corpus_gen.ts wiring).
// Corpus files are evidence: written on first run, byte-compared afterwards
// (staleness fails). Consumers: this file (TS) and the C++ nightly replay
// tests/fuzz/test_field_fuzz_parity.cpp.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

import { genFieldProgram, genFieldVectors } from '../src/field_ir/fuzz_gen.js';
import { decodeZprog } from '../src/field_ir/serialize.js';
import { decodeZvec } from '../src/field_ir/zvec.js';
import { interpret } from '../src/field_ir/interpret.js';
import { repoRoot } from './helpers.js';

const CORPUS_DIR = path.join(repoRoot(), 'tests', 'fuzz', 'corpus', 'field');
const SEEDS = [0x51CE, 0x5A17, 0xC0FF, 0xEE, 0x1234, 0xF00D, 0xBEEF, 0x7A17];
const VECTORS_PER_PROGRAM = 16;

function stableBinary(rel: string, bytes: Uint8Array): void {
  const abs = path.join(CORPUS_DIR, rel);
  if (!existsSync(abs)) {
    writeFileSync(abs, bytes);
    console.log(`[field-fuzz] WROTE ${rel} — commit it`);
    return;
  }
  assert.ok(Buffer.from(readFileSync(abs)).equals(Buffer.from(bytes)),
    `${rel} stale — regenerate and commit`);
}

test('field fuzz corpus: 8 programs, TS replay, committed bytes stable', () => {
  mkdirSync(CORPUS_DIR, { recursive: true });
  for (const seed of SEEDS) {
    const { bytes, hash, name } = genFieldProgram(seed);
    stableBinary(`${name}.zprog`, bytes);
    const zvec = genFieldVectors(bytes, hash, seed, VECTORS_PER_PROGRAM);
    stableBinary(`${name}.zvec`, zvec);

    // TS self-replay: decode both files and interpret every record
    const decRes = decodeZprog(Uint8Array.from(readFileSync(path.join(CORPUS_DIR, `${name}.zprog`))));
    if (!decRes.ok) throw new Error(`${name}: ${decRes.errors.join('; ')}`);
    const zdecRes = decodeZvec(Uint8Array.from(readFileSync(path.join(CORPUS_DIR, `${name}.zvec`))));
    if (!zdecRes.ok) throw new Error(`${name}: ${zdecRes.errors.join('; ')}`);
    assert.equal(zdecRes.zvec.programHash, hash);
    for (const rec of zdecRes.zvec.records) {
      const r = interpret(decRes.prog, rec.inputs);
      assert.deepEqual(r.outputs, rec.expected, `${name} outputs`);
      assert.equal((r.status.sat ? 1 : 0) | (r.status.rcp0 ? 2 : 0), rec.status,
        `${name} status`);
    }
  }
  console.log(`[field-fuzz] corpus: ${SEEDS.length} programs x ` +
    `${VECTORS_PER_PROGRAM} vectors green in TS`);
});
