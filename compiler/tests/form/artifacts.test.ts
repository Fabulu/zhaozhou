import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';

import {
  COST_SCHEMA,
  decodeSourceMap,
  emitCostReport,
  emitSourceMap,
  validateCostReport,
  ZMAP_ENTRY_BYTES,
  ZMAP_FILE_BYTES,
  ZMAP_HEADER_BYTES,
  ZMAP_MAGIC,
  ZMAP_VERSION,
} from '../../src/backends/index.js';
import { compileFrontend } from '../../src/frontend/index.js';
import { crc32c } from '../../src/generated/abi.js';
import { declarationsOf, lowerHir } from '../../src/hir/index.js';
import { lowerZir } from '../../src/zir/index.js';
import { repoRoot } from '../helpers.js';

function fixtureSources(reverse = false): Record<string, string> {
  const dir = path.join(repoRoot(), 'compiler', 'tests', 'form', 'fixture');
  const files = ['a_arena.form', 'b_audit.form'];
  if (reverse) files.reverse();
  return Object.fromEntries(files.map((file) => [file, readFileSync(path.join(dir, file), 'utf8')]));
}

function compileFixture(reverse = false) {
  const frontend = compileFrontend(fixtureSources(reverse));
  assert.equal(frontend.ok, true, frontend.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  return { hir, zir: lowerZir(hir) };
}

function physicalField(hir: NonNullable<ReturnType<typeof lowerHir>>) {
  const field = declarationsOf(hir, 'field')[0]!;
  return {
    profile: field.profile,
    sourceId: field.sourceId,
    outputs: [{ name: 'height', type: 'fx' as const, reg: 0, min: 0, max: 0 }],
    cost: {
      instrCount: 4,
      byClass: { ALU: 3, MUL: 1, TABLE: 0, NOISE: 0, SPECIAL: 0 },
      cycles: 5,
      dsp: 1,
      tableBytes: 0,
      regHighWater: 3,
    },
  };
}

function costOptions(hir: NonNullable<ReturnType<typeof lowerHir>>) {
  return {
    abiVersion: 2,
    commandMemoryCeilingBytes: 1_048_576,
    fieldPrograms: [physicalField(hir)],
    budgets: [{ line: 'frame_slot_bytes', limit: 1_048_576, owner: 'spec/commands.zidl' }],
  };
}

test('sourceids.zmap has exact v1 layout, ordering, and body CRC', () => {
  const { hir } = compileFixture();
  const first = emitSourceMap(hir);
  const second = emitSourceMap(hir);
  assert.deepEqual(first, second);

  const view = new DataView(first.buffer, first.byteOffset, first.byteLength);
  assert.equal(view.getUint32(0, true), ZMAP_MAGIC);
  assert.equal(view.getUint16(4, true), ZMAP_VERSION);
  assert.equal(view.getUint16(6, true), 0);
  assert.equal(view.getUint32(8, true), hir.sourceIds.length);
  assert.equal(view.getUint32(12, true), hir.modules.length);
  assert.equal(view.getBigUint64(24, true), 0n);
  assert.equal(view.getUint32(20, true), crc32c(0, first, ZMAP_HEADER_BYTES));
  const blobBytes = view.getUint32(16, true);
  assert.equal(first.length, ZMAP_HEADER_BYTES + hir.sourceIds.length * ZMAP_ENTRY_BYTES + hir.modules.length * ZMAP_FILE_BYTES + blobBytes);

  const decoded = decodeSourceMap(first);
  assert.deepEqual(decoded.files, ['a_arena.form', 'b_audit.form']);
  assert.deepEqual(decoded.entries, [...hir.sourceIds].sort((a, b) => a.sourceId - b.sourceId));
  assert.ok(decoded.entries.every((row, index, rows) => index === 0 || rows[index - 1]!.sourceId < row.sourceId));
});

test('sourceids.zmap rejects CRC and denormalized metadata corruption', () => {
  const { hir } = compileFixture();
  const valid = emitSourceMap(hir);
  const crcFault = valid.slice();
  crcFault[crcFault.length - 1] = crcFault[crcFault.length - 1]! ^ 0x80;
  assert.throws(() => decodeSourceMap(crcFault), /CRC mismatch/);

  const kindFault = valid.slice();
  kindFault[ZMAP_HEADER_BYTES + 6] = kindFault[ZMAP_HEADER_BYTES + 6]! ^ 1;
  new DataView(kindFault.buffer).setUint32(20, crc32c(0, kindFault, ZMAP_HEADER_BYTES), true);
  assert.throws(() => decodeSourceMap(kindFault), /kind mismatch/);

  const offsetFault = valid.slice();
  new DataView(offsetFault.buffer).setUint16(ZMAP_HEADER_BYTES + 16, 0xffff, true);
  new DataView(offsetFault.buffer).setUint32(20, crc32c(0, offsetFault, ZMAP_HEADER_BYTES), true);
  assert.throws(() => decodeSourceMap(offsetFault), /string offset outside blob/);
});

test('costs.zcost is complete canonical integer JSON and deterministic', () => {
  const first = compileFixture(false);
  const second = compileFixture(true);
  const bytes = emitCostReport(first.hir, first.zir, costOptions(first.hir));
  assert.deepEqual(bytes, emitCostReport(second.hir, second.zir, costOptions(second.hir)));
  const text = new TextDecoder().decode(bytes);
  assert.equal(text.at(-1), '\n');
  assert.equal(text.includes('\r'), false);
  assert.equal(text.slice(0, -1).includes('\n'), false);

  const report = validateCostReport(bytes);
  assert.equal(report.$schema, COST_SCHEMA);
  assert.equal(report.abi_version, 2);
  assert.deepEqual(report.modules, [{ index: 0, name: 'arena' }, { index: 1, name: 'audit' }]);
  assert.deepEqual(report.pools, [{ capacity: 4, element_bytes: 29, module: 0, name: 'particles' }]);
  assert.deepEqual(report.rates, [
    { every: 2, module: 0, name: 'seed_wave', phase: 0, stagger: false },
    { every: 4, module: 0, name: 'advance', phase: 1, stagger: true },
    { every: 1, module: 1, name: 'observe', phase: 0, stagger: false },
  ]);
  assert.equal(report.command_memory.per_frame_estimate_bytes, 64);
  assert.deepEqual(report.particle_bandwidth, { bytes_per_element: 0, peak_elements: 0, bytes_per_tick: 0, pools: [] });
  assert.deepEqual(report.programs[0]!.class_counts, { ALU: 3, MUL: 1, NOISE: 0, SPECIAL: 0, TABLE: 0 });
  assert.equal(report.programs[0]!.instr_count, 4);
  assert.deepEqual(report.programs[0]!.footprint_rect, declarationsOf(first.hir, 'field')[0]!.footprint.rect.map(Number));
});

test('costs.zcost validator refuses noncanonical and nonschema bytes', () => {
  const { hir, zir } = compileFixture();
  const bytes = emitCostReport(hir, zir, costOptions(hir));
  const report = JSON.parse(new TextDecoder().decode(bytes));
  const pretty = new TextEncoder().encode(`${JSON.stringify(report, null, 2)}\n`);
  assert.throws(() => validateCostReport(pretty), /not canonical/);

  report.abi_version = 2.5;
  const fractional = new TextEncoder().encode(`${JSON.stringify(report)}\n`);
  assert.throws(() => validateCostReport(fractional), /unsigned integer/);

  assert.throws(
    () => emitCostReport(hir, zir, { ...costOptions(hir), fieldPrograms: [] }),
    /missing physical Field IR/,
  );
});
