// crater_ring.test.ts — W5 acceptance, TS side (spec/form/field-ir.md §12
// checks 1 and the byte-stability gate): build → validate → serialize → emit
// the C++ wrapper. The emitted artifacts are COMMITTED (they are evidence);
// this test regenerates them and fails on drift, and writes them on first run.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

import { buildCraterRing } from '../src/field_ir/crater_ring.js';
import { emitCppWrapper } from '../src/field_ir/emit_cpp.js';
import { repoRoot } from './helpers.js';

const GENERATED_DIR = path.join(repoRoot(), 'compiler', 'tests', 'generated');

function stableFile(rel: string, bytes: Buffer | string, note: string): void {
  const abs = path.join(GENERATED_DIR, rel);
  if (!existsSync(abs)) {
    writeFileSync(abs, bytes);
    console.log(`[crater_ring] WROTE ${rel} (${note}) — commit it`);
    return;
  }
  const committed = readFileSync(abs);
  const want = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes, 'utf8');
  assert.ok(commutedEquals(committed, want),
    `${rel} is stale — regenerate and commit (field-ir.md §11.3 determinism)`);
}

function commutedEquals(a: Buffer, b: Buffer): boolean {
  return a.equals(b);
}

test('crater_ring: emit + commit the C++ wrapper and .zprog (§12 check 1)', () => {
  mkdirSync(GENERATED_DIR, { recursive: true });
  const { program, bytes, hash, ringPc, ringSpan } = buildCraterRing();

  // sanity: earth ceiling, expected shape
  assert.ok(program.code.length <= 32, `instrs ${program.code.length} > 32`);
  assert.equal(program.code[0]!.op, 'DIST2');
  assert.ok(program.code.some((i) => i.op === 'RING'));
  assert.ok(program.code.some((i) => i.op === 'CURVE'));
  assert.ok(program.code.some((i) => i.op === 'DCURVE'));
  assert.equal(program.inputs.length, 12);
  assert.equal(program.outputs.length, 4);
  assert.deepEqual(program.outputs.map((o) => o.name),
    ['height', 'velocity', 'material', 'nav_cost']);

  const wrapper = emitCppWrapper(program, bytes, hash, {
    namespace: 'crater_ring',
    sourceName: 'spells/upheaval.form (crater_ring earth program)',
    markers: [{ name: 'ring', pc: ringPc, sourceId: ringSpan.sourceId,
                line: ringSpan.line, col: ringSpan.col }],
  });
  stableFile('crater_ring.hpp', wrapper, 'generated typed wrapper');
  stableFile('crater_ring.zprog', Buffer.from(bytes), 'serialized program');

  // the wrapper embeds the hash and the RING marker
  assert.ok(wrapper.includes(`kProgramHash = 0x${(hash >>> 0).toString(16)}`));
  assert.ok(wrapper.includes('kRING_Pc'));
  console.log(`[crater_ring] program hash = 0x${(hash >>> 0).toString(16)}` +
    ` (${program.code.length} instrs, RING at pc ${ringPc})`);
});

test('crater_ring: cost report sanity (§9)', () => {
  const { program } = buildCraterRing();
  const c = program.cost;
  assert.equal(c.instrCount, program.code.length);
  assert.equal(
    c.byClass.ALU + c.byClass.MUL + c.byClass.TABLE + c.byClass.NOISE + c.byClass.SPECIAL,
    c.instrCount);
  assert.ok(c.cycles > 0 && c.cycles < 200);
  assert.equal(c.tableBytes, 4 + 12 * 8);   // one 8-entry table
  assert.ok(c.regHighWater > 12 && c.regHighWater <= 64);
});
