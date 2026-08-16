// wave_programs.test.ts — the Tier-1 deformation-reel field programs
// (wave_pool, impact_wave): build → validate → serialize → emit the C++
// wrappers, committed like crater_ring's (spec/form/field-ir.md §11.3
// byte-stability: this test regenerates them and fails on drift).

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

import { buildWavePool } from '../src/field_ir/wave_pool.js';
import { buildImpactWave } from '../src/field_ir/impact_wave.js';
import { emitCppWrapper } from '../src/field_ir/emit_cpp.js';
import { repoRoot } from './helpers.js';

const GENERATED_DIR = path.join(repoRoot(), 'compiler', 'tests', 'generated');

function stableFile(rel: string, bytes: Buffer | string, note: string): void {
  const abs = path.join(GENERATED_DIR, rel);
  if (!existsSync(abs)) {
    writeFileSync(abs, bytes);
    console.log(`[wave_programs] WROTE ${rel} (${note}) — commit it`);
    return;
  }
  const committed = readFileSync(abs);
  const want = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes, 'utf8');
  assert.ok(committed.equals(want),
    `${rel} is stale — regenerate and commit (field-ir.md §11.3 determinism)`);
}

test('wave_pool: emit + commit the C++ wrapper and .zprog', () => {
  mkdirSync(GENERATED_DIR, { recursive: true });
  const { program, bytes, hash, sinPc, sinSpan } = buildWavePool();

  // sanity: earth ceiling, the travelling-wave shape
  assert.ok(program.code.length <= 32, `instrs ${program.code.length} > 32`);
  assert.equal(program.code[0]!.op, 'DIST2');
  assert.ok(program.code.some((i) => i.op === 'SIN'));
  assert.ok(program.code.some((i) => i.op === 'COS'));
  assert.equal(program.inputs.length, 12);
  assert.equal(program.outputs.length, 4);
  assert.deepEqual(program.outputs.map((o) => o.name),
    ['height', 'velocity', 'material', 'nav_cost']);

  const wrapper = emitCppWrapper(program, bytes, hash, {
    namespace: 'wave_pool',
    sourceName: 'spells/membrane.form (wave_pool earth program)',
    markers: [{ name: 'sin', pc: sinPc, sourceId: sinSpan.sourceId,
                line: sinSpan.line, col: sinSpan.col }],
  });
  stableFile('wave_pool.hpp', wrapper, 'generated typed wrapper');
  stableFile('wave_pool.zprog', Buffer.from(bytes), 'serialized program');

  assert.ok(wrapper.includes(`kProgramHash = 0x${(hash >>> 0).toString(16)}`));
  console.log(`[wave_pool] program hash = 0x${(hash >>> 0).toString(16)}` +
    ` (${program.code.length} instrs, SIN at pc ${sinPc})`);
});

test('impact_wave: emit + commit the C++ wrapper and .zprog', () => {
  mkdirSync(GENERATED_DIR, { recursive: true });
  const { program, bytes, hash, waveletPc, waveletSpan } = buildImpactWave();

  assert.ok(program.code.length <= 32, `instrs ${program.code.length} > 32`);
  assert.equal(program.code[0]!.op, 'DIST2');
  assert.ok(program.code.some((i) => i.op === 'CURVE'));
  assert.ok(program.code.some((i) => i.op === 'DCURVE'));
  assert.equal(program.tables.length, 3);
  assert.equal(program.inputs.length, 12);
  assert.equal(program.outputs.length, 4);
  assert.deepEqual(program.outputs.map((o) => o.name),
    ['height', 'velocity', 'material', 'nav_cost']);

  const wrapper = emitCppWrapper(program, bytes, hash, {
    namespace: 'impact_wave',
    sourceName: 'spells/impact.form (impact_wave earth program)',
    markers: [{ name: 'wavelet', pc: waveletPc, sourceId: waveletSpan.sourceId,
                line: waveletSpan.line, col: waveletSpan.col }],
  });
  stableFile('impact_wave.hpp', wrapper, 'generated typed wrapper');
  stableFile('impact_wave.zprog', Buffer.from(bytes), 'serialized program');

  assert.ok(wrapper.includes(`kProgramHash = 0x${(hash >>> 0).toString(16)}`));
  console.log(`[impact_wave] program hash = 0x${(hash >>> 0).toString(16)}` +
    ` (${program.code.length} instrs, wavelet CURVE at pc ${waveletPc})`);
});

test('wave programs: cost report sanity (§9)', () => {
  for (const [name, build] of
       [['wave_pool', buildWavePool], ['impact_wave', buildImpactWave]] as const) {
    const { program } = build();
    const c = program.cost;
    assert.equal(c.instrCount, program.code.length, name);
    assert.equal(
      c.byClass.ALU + c.byClass.MUL + c.byClass.TABLE + c.byClass.NOISE + c.byClass.SPECIAL,
      c.instrCount, name);
    assert.ok(c.cycles > 0 && c.cycles < 200, name);
    assert.ok(c.regHighWater > 12 && c.regHighWater <= 64, name);
  }
});
