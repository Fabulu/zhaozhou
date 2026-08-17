import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';

import { compileFrontend } from '../../src/frontend/index.js';
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
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  return { frontend, hir, zir: lowerZir(hir) };
}

test('HIR is resolved, typed, domain-tagged, source-attributed', () => {
  const { hir } = compileFixture();
  assert.deepEqual(hir.modules.map((module) => [module.index, module.name, module.file]), [
    [0, 'arena', 'a_arena.form'],
    [1, 'audit', 'b_audit.form'],
  ]);
  assert.deepEqual(hir.declarations.map((decl) => decl.domain), [
    'constant', 'constant', 'state', 'state', 'state', 'sim', 'sim', 'field',
    'present', 'present', 'test', 'state', 'sim',
  ]);
  const globals = declarationsOf(hir, 'global');
  assert.equal(globals.find((global) => global.name === 'origin')?.init.type.t, 'world3');
  const observe = declarationsOf(hir, 'system').find((system) => system.name === 'observe')!;
  const assignment = observe.body[0]!;
  assert.equal(assignment.expressions[1]!.symbol?.kind, 'global');
  assert.equal(assignment.expressions[1]!.symbol?.module, 0);
  assert.equal(assignment.expressions[1]!.type.t, 'u32');
  assert.ok(hir.sourceIds.every((row) => row.span.end > row.span.start));
  assert.deepEqual(hir.sourceIds.map((row) => row.kind).sort((a, b) => a - b), [3, 8, 8, 8, 9, 9, 10, 11]);
});

test('HIR refuses to exist after frontend diagnostics', () => {
  const result = compileFrontend({
    'bad.form': 'module bad { global x: u32 = 0; system broken every 0 ticks reads writes x { x = 1; } }\n',
  });
  assert.equal(result.ok, false);
  assert.equal(lowerHir(result), null);
});

test('ZIR schedule is canonical across source-map insertion order', () => {
  const first = compileFixture(false).zir;
  const second = compileFixture(true).zir;
  const view = (zir: typeof first) => zir.sim.phases.map((phase) => ({
    index: phase.index,
    systems: phase.systems.map((system) => `${system.module}.${system.name}`),
  }));
  assert.deepEqual(view(first), view(second));
  assert.deepEqual(view(first), [
    { index: 0, systems: ['0.seed_wave', '1.observe'] },
    { index: 1, systems: ['0.advance'] },
  ]);
});

test('multi-rate and stagger lower to compile-time ZIR constants', () => {
  const { zir } = compileFixture();
  const seed = zir.sim.callList.find((system) => system.name === 'seed_wave')!;
  const advance = zir.sim.callList.find((system) => system.name === 'advance')!;
  assert.deepEqual(seed.rateGuard, { divisor: 2, remainder: 0 });
  assert.deepEqual(advance.rateGuard, { divisor: 4, remainder: 0 });
  assert.deepEqual(advance.stagger, { module: 0, pool: 'particles', divisor: 4 });
  assert.equal(7 % advance.stagger.divisor, 15 % advance.stagger.divisor);
  assert.equal(zir.present.perFrameEstimateBytes, 64);
});

test('conflicting writer diagnostic cites both declaration spans through W3.2 contract', () => {
  const source = `module conflict {
  global value: u32 = 0;
  system first every 1 ticks reads writes value { value = 1; }
  system second every 1 ticks reads writes value { value = 2; }
}\n`;
  const result = compileFrontend({ 'conflict.form': source });
  assert.equal(result.ok, false);
  const diagnostic = result.diagnostics.find((item) => item.code === 'FORM-E-500');
  assert.ok(diagnostic);
  const firstStart = source.indexOf('system first');
  const secondWriteStart = source.lastIndexOf('value', source.indexOf('{ value = 2'));
  assert.match(diagnostic.message, /'first'/);
  assert.match(diagnostic.message, /'second'/);
  assert.match(diagnostic.message, new RegExp(`conflict\\.form:${firstStart}`));
  assert.match(diagnostic.message, new RegExp(`conflict\\.form:${secondWriteStart}`));
});
