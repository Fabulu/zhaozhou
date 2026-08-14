// field_ts_differential.test.ts — the TS interpreter replays the C++-owned
// golden .zvec BYTE-IDENTICALLY (field-ir.md §10, §12 check 7; Csmith-style
// subordinate differential — also continuously validates the serializer and
// the pinned §6.2 input generator across languages). Additionally replays
// the committed minimize-demo failure artifact (§6.3).

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';

import { buildCraterRing } from '../src/field_ir/crater_ring.js';
import { decodeZprog } from '../src/field_ir/serialize.js';
import { decodeZvec, generateInputs, minimizeRecord, LaneBounds } from '../src/field_ir/zvec.js';
import { interpret } from '../src/field_ir/interpret.js';
import { repoRoot } from './helpers.js';

const GOLDEN = path.join(repoRoot(), 'captures', 'golden', 'field', 'crater_ring.zvec');
const SEED = 0x5a17;

test('TS interpreter replays the C++ golden .zvec byte-identically (§12 check 7)', {
  skip: !existsSync(GOLDEN) &&
    'golden .zvec not yet generated — run ctest field_crater_ring first (W5 order)',
}, () => {
  const { bytes } = buildCraterRing();
  const decRes = decodeZprog(bytes);
  if (!decRes.ok) throw new Error(decRes.errors.join('; '));
  const dec = decRes.prog;
  const zdecRes = decodeZvec(Uint8Array.from(readFileSync(GOLDEN)));
  if (!zdecRes.ok) throw new Error(zdecRes.errors.join('; '));
  const zvec = zdecRes.zvec;
  assert.equal(zvec.programHash, dec.programHash);
  assert.equal(zvec.seed, SEED);

  // §6.2 cross-language generator check: regenerate the inputs from
  // (hash, seed, N) and compare with what the C++ oracle recorded
  const bounds: LaneBounds[] = dec.inLanes.map((l) => ({ min: l.min, max: l.max }));
  const n = zvec.records.length - 3 - bounds.length;
  const regen = generateInputs(zvec.programHash, SEED, n, bounds);
  assert.equal(regen.length, zvec.records.length);
  for (let i = 0; i < regen.length; i++) {
    assert.deepEqual(regen[i], zvec.records[i]!.inputs,
      `input record ${i} identical across TS/C++ generators`);
  }

  // the differential proper: every record's expected lanes + status
  let worst = 0;
  for (let i = 0; i < zvec.records.length; i++) {
    const rec = zvec.records[i]!;
    const r = interpret(dec, rec.inputs);
    assert.deepEqual(r.outputs, rec.expected,
      `record ${i}: TS outputs diverge from the C++ golden`);
    const st = (r.status.sat ? 1 : 0) | (r.status.rcp0 ? 2 : 0);
    assert.equal(st, rec.status, `record ${i}: status word diverges`);
    worst = Math.max(worst, rec.expected.length);
  }
  console.log(`[field-ts-diff] ${zvec.records.length} records ` +
    `(${worst} lanes) byte-identical to the C++ oracle`);
});

test('failure artifact: committed minimize demo replays (§6.3)', () => {
  const { bytes, hash } = buildCraterRing();
  const decRes = decodeZprog(bytes);
  if (!decRes.ok) throw new Error(decRes.errors.join('; '));
  const prog = decRes.prog;
  const failPath = path.join(repoRoot(), 'captures', 'failures', 'field',
    `fail-${(hash >>> 0).toString(16).padStart(8, '0')}-0x5A17.zvec`);
  if (!existsSync(failPath)) {
    console.log('[field-ts-diff] failure artifact not yet generated — ' +
      'run ctest field_crater_ring (W5 order)');
    return;
  }
  const zdecRes = decodeZvec(Uint8Array.from(readFileSync(failPath)));
  if (!zdecRes.ok) throw new Error(zdecRes.errors.join('; '));
  assert.equal(zdecRes.zvec.records.length, 1);
  const rec = zdecRes.zvec.records[0]!;
  const bounds = prog.inLanes.map((l) => ({ min: l.min, max: l.max }));
  const fails = (inputs: number[]): boolean => {
    const r = interpret(prog, inputs);
    return r.outputs[0] !== rec.expected[0];
  };
  assert.ok(fails(rec.inputs), 'artifact record still fails on replay');
  // §6.3 minimize from the artifact's own record converges to the same input
  // (the committed record IS the minimized form)
  const minimized = minimizeRecord(rec.inputs, bounds, fails);
  assert.ok(fails(minimized));
  assert.deepEqual(minimized, rec.inputs,
    'TS minimize reproduces the committed minimized record');
});
