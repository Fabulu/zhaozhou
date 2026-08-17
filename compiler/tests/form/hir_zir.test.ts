import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';

import { compileFrontend } from '../../src/frontend/index.js';
import { declarationsOf, lowerHir } from '../../src/hir/index.js';
import {
  SOURCE_KIND_EMIT, SOURCE_KIND_POOL, SOURCE_KIND_SCENARIO, SOURCE_KIND_SYSTEM,
} from '../../src/hir/lower.js';
import type { HirExpr, HirStmt } from '../../src/hir/model.js';
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

test('Form source-kind references match the authoritative capture-format registry', () => {
  const capture = readFileSync(path.join(repoRoot(), 'spec', 'capture_format.md'), 'utf8');
  const semantics = readFileSync(path.join(repoRoot(), 'spec', 'form', 'language-semantics.md'), 'utf8');
  const domains = readFileSync(path.join(repoRoot(), 'spec', 'form', 'domains-and-effects.md'), 'utf8');
  const authoritative = capture.match(
    /\*\*\[w3\]\*\* (\d+) system, (\d+) presentation emit site, (\d+) pool, (\d+) scenario/,
  );
  assert.ok(authoritative, 'capture_format.md §5 must carry the source-kind registry');
  assert.deepEqual(authoritative.slice(1).map(Number), [
    SOURCE_KIND_SYSTEM, SOURCE_KIND_EMIT, SOURCE_KIND_POOL, SOURCE_KIND_SCENARIO,
  ]);
  assert.match(semantics, /source ID \(kind \*\*9\*\*; the registry in `capture_format\.md` §5 is\s+authoritative\)/);
  assert.match(domains, /authoritative source-kind registry is `capture_format\.md` §5:[\s\S]*kind \*\*9\*\*[\s\S]*kind \*\*8\*\*[\s\S]*kind \*\*10\*\*[\s\S]*kind \*\*11\*\*/);
  assert.doesNotMatch(semantics + domains, /emit site[^\n]*kind 6|systems are kind 5|pools kind 7|scenarios kind 8/);
});

test('HIR preserves exact contextual types through every L1 target context', () => {
  const frontend = compileFrontend({
    'types.form': `module types {
  struct packet { si: i32; ui: u32; q16: fx16; q24: fx24; turn: angle16; weight: unit8; yes: bool; }
  pool packets: packet[2];
  global initial: packet = packet { si = -1, ui = 1, q16 = 2, q24 = 3, turn = 90deg, weight = 50%, yes = true };
  global position: world3 = world3 { x = 1, y = 2, z = 3 };
  global total: u32 = 0;
  fn select_u(flag: bool, a: u32, b: u32) -> u32 { return if flag { 1 } else { a + b }; }
  system update every 1 ticks reads packets, total writes packets.ui, total {
    for i in 0..packets.count { packets.ui[i] = select_u(true, 1, 2); }
    total = total + 1;
  }
}\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);

  const initial = declarationsOf(hir, 'global').find((item) => item.name === 'initial')!;
  assert.deepEqual(initial.init.children.map((child) => child.type.t),
    ['i32', 'u32', 'fx16', 'fx24', 'angle16', 'unit8', 'bool']);
  const position = declarationsOf(hir, 'global').find((item) => item.name === 'position')!;
  assert.deepEqual(position.init.children.map((child) => child.type.t), ['fx24', 'fx24', 'fx24']);

  const select = declarationsOf(hir, 'fn').find((item) => item.name === 'select_u')!;
  const selectExpr = select.body[0]!.expressions[0]!;
  assert.equal(selectExpr.type.t, 'u32');
  assert.deepEqual(selectExpr.children.map((child) => child.type.t), ['bool', 'u32', 'u32']);
  assert.deepEqual(selectExpr.children[2]!.children.map((child) => child.type.t), ['u32', 'u32']);

  const update = declarationsOf(hir, 'system').find((item) => item.name === 'update')!;
  const poolAssign = update.body[0]!.body[0]!;
  assert.deepEqual(poolAssign.expressions.map((expr) => expr.type.t), ['u32', 'u32']);
  assert.deepEqual(poolAssign.expressions[1]!.children.map((child) => child.type.t), ['bool', 'u32', 'u32']);
  const globalAssign = update.body[1]!;
  assert.deepEqual(globalAssign.expressions.map((expr) => expr.type.t), ['u32', 'u32']);
  assert.deepEqual(globalAssign.expressions[1]!.children.map((child) => child.type.t), ['u32', 'u32']);
});

test('HIR uses the frontend exact reducer for required bounds and width-normalized constants', () => {
  const frontend = compileFrontend({
    'capacity.form': `module capacity {
      const ZERO: u32 = 0;
      const MASK: u32 = ~ZERO;
      const CAP: u32 = MASK >> 30;
      const WRAPPED: u32 = 4294967295 + 2;
      const SIGNED: i32 = ~0;
      const TRUTH: bool = !false;
      struct row { values: u32[CAP]; }
      pool rows: row[CAP];
    }\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const constants = new Map(declarationsOf(hir, 'const').map((item) => [item.name, item.raw]));
  assert.deepEqual(constants, new Map<string, bigint | null>([
    ['ZERO', 0n], ['MASK', 0xffffffffn], ['CAP', 3n],
    ['WRAPPED', 1n], ['SIGNED', -1n], ['TRUTH', 1n],
  ]));
  assert.equal(declarationsOf(hir, 'pool')[0]!.capacity, 3);
  const row = declarationsOf(hir, 'struct')[0]!;
  assert.equal(row.fields[0]!.type.t, 'array');
  assert.equal(row.fields[0]!.type.t === 'array' ? row.fields[0]!.type.len : null, 3);

  const refused = compileFrontend({
    'irreducible.form': `module irreducible {
      const ZERO: u32 = 0;
      const BAD: u32 = 1 / ZERO;
      struct row { values: u32[BAD]; }
      pool rows: row[BAD];
    }\n`,
  });
  assert.equal(refused.ok, false);
  assert.ok(refused.diagnostics.some((item) => item.code === 'FORM-E-800'));
  assert.ok(refused.diagnostics.some((item) => item.code === 'FORM-E-801'));
  assert.equal(lowerHir(refused), null);
});

test('HIR preserves exact positive and negative Q-format rails without pre-negation saturation', () => {
  const hugeDenominator = '0'.repeat(400);
  const frontend = compileFrontend({
    'rails.form': `module rails {
      const F16_LO: fx16 = -32768m;
      const F16_HI: fx16 = 32767.9999847412109375m;
      const F16_STEP: fx16 = 0.0000152587890625m;
      const F24_LO: fx24 = -8192w;
      const F24_HI: fx24 = 8191.999999940395355224609375w;
      const F24_STEP: fx24 = 0.000000059604644775390625w;
      const HUGE_ONE: fx16 = 1.${hugeDenominator}m;
    }\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  assert.deepEqual(new Map(declarationsOf(hir, 'const').map((item) => [item.name, item.raw])),
    new Map<string, bigint | null>([
      ['F16_LO', -2147483648n], ['F16_HI', 2147483647n], ['F16_STEP', 1n],
      ['F24_LO', -137438953472n], ['F24_HI', 137438953471n], ['F24_STEP', 1n],
      ['HUGE_ONE', 65536n],
    ]));
});

test('HIR shares exact nested aggregate projection with checker bounds without fallback values', () => {
  const frontend = compileFrontend({
    'a_owner.form': `module owner {
      struct leaf { cap: u32; }
      struct metadata { nested: leaf; }
      const META: metadata = metadata { nested = leaf { cap = 5 } };
    }\n`,
    'b_aggregate.form': `module aggregate {
      import owner;
      struct leaf { cap: u32; }
      struct metadata { nested: leaf; }
      const FORWARD: u32 = LATER.nested.cap;
      const LATER: metadata = metadata { nested = leaf { cap = 3 } };
      const DIRECT: u32 = (metadata { nested = leaf { cap = 4 } }).nested.cap;
      const IMPORTED: u32 = owner.META.nested.cap;
      const CAP: u32 = FORWARD + DIRECT + IMPORTED;
      struct row { values: u32[CAP]; }
      pool rows: row[CAP];
    }\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const constants = new Map(declarationsOf(hir, 'const')
    .filter((item) => item.module === 1)
    .map((item) => [item.name, item.raw]));
  assert.deepEqual(constants, new Map<string, bigint | null>([
    ['FORWARD', 3n], ['LATER', null], ['DIRECT', 4n], ['IMPORTED', 5n], ['CAP', 12n],
  ]));
  assert.equal(declarationsOf(hir, 'pool').find((item) => item.module === 1)?.capacity, 12);
  const row = declarationsOf(hir, 'struct').find((item) => item.module === 1 && item.name === 'row')!;
  assert.equal(row.fields[0]!.type.t === 'array' ? row.fields[0]!.type.len : null, 12);
});

test('qualified flow calls retain owner identity and canonical pool effects in HIR', () => {
  const frontend = compileFrontend({
    'a_flowlib.form': `module flowlib {
      @flow field drift() -> flow_update footprint none; max_ops 48 {
        return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0m };
      }
    }\n`,
    'b_consumer.form': `module consumer {
      import flowlib;
      struct particle { position: world3; velocity: velocity3; age: u32; }
      pool motes: particle[4];
      system move every 1 ticks reads motes writes motes { flowlib.drift(motes); }
    }\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const system = declarationsOf(hir, 'system')[0]!;
  assert.deepEqual(system.reads, ['consumer\0motes']);
  assert.deepEqual(system.writes, ['consumer\0motes']);
  assert.deepEqual(system.body[0]!.expressions[0]!.symbol, { kind: 'field', module: 0, name: 'drift' });
  assert.deepEqual(system.body[0]!.expressions[0]!.children[0]!.symbol,
    { kind: 'pool', module: 1, name: 'motes' });
});

test('HIR keeps ordinary composition recursive, marks only direct pool columns, and starts source rows at zero', () => {
  const frontend = compileFrontend({
    'composition.form': `module composition {
  const TWO: u32 = 2;
  struct inner { value: u32; }
  struct outer { payload: inner; }
  struct row { count: u32; lanes: u32[TWO]; }
  global box: outer = outer { payload = inner { value = 7 } };
  global observed: u32 = 0;
  pool items: row[3];
  system inspect every 1 ticks reads box, items writes observed {
    let nested = box.payload.value;
    let membership = items.count;
    let authored_count = items.count[0];
    let nested_index = items.lanes[0][1];
    observed = nested + membership + authored_count + nested_index;
  }
}\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const pool = declarationsOf(hir, 'pool')[0]!;
  const system = declarationsOf(hir, 'system')[0]!;
  assert.equal(pool.sourceId, (SOURCE_KIND_POOL << 28) >>> 0);
  assert.equal(system.sourceId, ((SOURCE_KIND_SYSTEM << 28) | 1) >>> 0);
  assert.deepEqual(hir.sourceIds.map((row) => row.sourceId & 0xffff).sort((a, b) => a - b), [0, 1]);
  assert.deepEqual(system.reads, ['composition\0box', 'composition\0items']);
  assert.deepEqual(system.writes, ['composition\0observed']);

  const nested = system.body[0]!.expressions[0]!;
  assert.equal(nested.ast.kind, 'member');
  assert.equal(nested.symbol, null);
  assert.equal(nested.poolColumn, null);
  assert.equal(nested.children[0]!.symbol, null);
  assert.deepEqual(nested.children[0]!.children[0]!.symbol, { kind: 'global', module: 0, name: 'box' });

  const membership = system.body[1]!.expressions[0]!;
  assert.deepEqual(membership.symbol, { kind: 'pool', module: 0, name: 'items' });
  assert.equal(membership.poolColumn, null);

  const authoredCount = system.body[2]!.expressions[0]!;
  assert.equal(authoredCount.ast.kind, 'index');
  assert.equal(authoredCount.symbol, null);
  assert.deepEqual(authoredCount.children[0]!.poolColumn, { module: 0, pool: 'items', field: 'count' });
  assert.equal(authoredCount.children[0]!.symbol, null);

  const nestedIndex = system.body[3]!.expressions[0]!;
  assert.equal(nestedIndex.ast.kind, 'index');
  assert.equal(nestedIndex.children[0]!.ast.kind, 'index');
  assert.deepEqual(nestedIndex.children[0]!.children[0]!.poolColumn,
    { module: 0, pool: 'items', field: 'lanes' });
  assert.equal(nestedIndex.poolColumn, null);
});

test('TestZIR explicitly lowers every scenario operation with resolved owner identity', () => {
  const frontend = compileFrontend({
    'scenario_ops.form': `module scenario_ops {
  global origin: world3 = world3 { x = 1w, y = 2w, z = 3w };
  global counter: u32 = 0;
  system step every 1 ticks reads counter writes counter { counter = counter + 1; }
  presentation limits { view 0 from origin budget 100%; }
  scenario all_operations {
    seed 42;
    load scenario_ops;
    spawn player 2 at origin;
    at 5 ticks step();
    assert counter == 1 within 0.25;
    capture frame 9 as "snapshot";
    assert_budget limits;
  }
}\n`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const zir = lowerZir(hir);
  assert.equal(zir.test.scenarios.length, 1);
  const scenario = zir.test.scenarios[0]!;
  assert.deepEqual(scenario.operations.map((operation) => operation.kind),
    ['seed', 'load', 'spawn_player', 'at', 'assert', 'capture', 'assert_budget']);
  assert.deepEqual(scenario.operations[0], {
    kind: 'seed', value: 42, span: scenario.operations[0]!.span,
  });
  assert.deepEqual(scenario.operations[1], {
    kind: 'load', module: 0, name: 'scenario_ops', span: scenario.operations[1]!.span,
  });
  assert.deepEqual(scenario.operations[2], {
    kind: 'spawn_player', player: 2, placement: { module: 0, global: 'origin' },
    span: scenario.operations[2]!.span,
  });
  const at = scenario.operations[3]!;
  assert.equal(at.kind, 'at');
  if (at.kind === 'at') {
    assert.equal(at.tick, 5);
    assert.deepEqual(at.system, { module: 0, name: 'step', sourceId: declarationsOf(hir, 'system')[0]!.sourceId });
  }
  const assertion = scenario.operations[4]!;
  assert.equal(assertion.kind, 'assert');
  if (assertion.kind === 'assert') {
    assert.equal(assertion.expression.type.t, 'bool');
    assert.equal(assertion.tolerance?.type.t, 'fx16');
  }
  assert.deepEqual(scenario.operations[5], {
    kind: 'capture', frame: 9, name: 'snapshot', span: scenario.operations[5]!.span,
  });
  assert.deepEqual(scenario.operations[6], {
    kind: 'assert_budget', presentation: { module: 0, name: 'limits' },
    span: scenario.operations[6]!.span,
  });
});

test('imported nominal types retain their defining owner without importing the type name', () => {
  const frontend = compileFrontend({
    'a_owner.form': `module owner {
  struct hidden_record { value: u32; }
  enum hidden_mode { ready = 1, done = 2 }
  global payload_state: hidden_record = hidden_record { value = 7 };
  global current: hidden_mode = hidden_mode.ready;
  pool entries: hidden_record[3];
  fn echo(value: hidden_record) -> hidden_record { return value; }
  fn echo_mode(value: hidden_mode) -> hidden_mode { return value; }
}
`,
    'b_consumer.form': `module consumer {
  import owner { payload_state, current, entries, echo, echo_mode };
  system use_imports every 1 ticks reads payload_state, current, entries writes payload_state {
    payload_state = echo(payload_state);
    let selected = echo_mode(current);
    let same = selected == current;
    for entry in entries { let observed = entry.value; }
  }
}
`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const nominalTypes = [...frontend.check!.expressionTypes.values()].filter(
    (type) => type.t === 'struct' || type.t === 'enum' || type.t === 'pool',
  );
  assert.ok(nominalTypes.length > 0);
  for (const type of nominalTypes) {
    assert.equal(type.owner, 'owner');
    if (type.t === 'pool') assert.equal(type.structOwner, 'owner');
  }

  const hir = lowerHir(frontend);
  assert.ok(hir);
  const pool = declarationsOf(hir, 'pool').find((item) => item.name === 'entries')!;
  assert.equal(pool.structModule, 0);
  const system = declarationsOf(hir, 'system').find((item) => item.name === 'use_imports')!;
  assert.deepEqual(system.body[0]!.expressions.map((expr) => expr.type), [
    { t: 'struct', name: '0::hidden_record' },
    { t: 'struct', name: '0::hidden_record' },
  ]);
  assert.deepEqual(system.body[1]!.expressions[0]!.type, { t: 'enum', name: '0::hidden_mode' });
  assert.equal(system.body[2]!.expressions[0]!.type.t, 'bool');
  assert.equal(system.body[3]!.body[0]!.expressions[0]!.type.t, 'u32');
});

test('field-table operands retain an exact symbolic non-runtime category', () => {
  const frontend = compileFrontend({
    'tables.form': `module tables {
  @flow field sample_curve() -> flow_update footprint none; max_ops 16 {
    let shaped = spline(swing_table, p.dt);
    return flow_update {
      x = p.x, y = p.y, z = p.z,
      vx = p.vx, vy = p.vy, vz = p.vz, attr0 = shaped,
    };
  }
}
`,
  });
  assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const hir = lowerHir(frontend);
  assert.ok(hir);
  const field = declarationsOf(hir, 'field')[0]!;
  const spline = field.body[0]!.expressions[0]!;
  assert.equal(spline.symbol?.name, 'spline');
  assert.equal(spline.type.t, 'fx16');
  assert.equal(spline.children[0]!.type.t, 'field_table');
  assert.deepEqual(spline.children[0]!.symbol, {
    kind: 'field_table', module: null, name: 'swing_table',
  });
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
  assert.equal(advance.rateGuard, null);
  assert.deepEqual(advance.stagger, { module: 0, pool: 'particles', divisor: 4 });
  assert.equal(7 % advance.stagger.divisor, 15 % advance.stagger.divisor);
  assert.deepEqual(zir.present.layouts.map((layout) => ({
    module: layout.module,
    presentation: layout.presentation,
    viewIds: layout.views.map((view) => view.id),
    budgets: layout.views.map((view) => view.budgetPct),
    shared: layout.sharedBudgetPct,
  })), [{ module: 0, presentation: 'main_view', viewIds: [0], budgets: [80], shared: 20 }]);
  assert.equal(zir.present.perFrameEstimateBytes, 208);
});

test('RNG call sites receive stable canonical HIR slots carried into SimZIR', () => {
  const sources = {
    'rng.form': `module rng {
  global result: u32 = 0;
  system draw_rng every 1 ticks reads writes result {
    let first = random.stream(11, 1);
    let second = random.stream(22, 2);
    result = random.u32(first) ^ random.u32(second);
  }
}
`,
  };
  const compile = () => {
    const frontend = compileFrontend(sources);
    assert.equal(frontend.ok, true, frontend.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
    const hir = lowerHir(frontend);
    assert.ok(hir);
    return { hir, zir: lowerZir(hir) };
  };
  const first = compile();
  const second = compile();
  const slots = (program: typeof first.hir): number[] => {
    const found: number[] = [];
    const expression = (value: HirExpr): void => {
      if (value.rngSlot !== null) found.push(value.rngSlot);
      for (const child of value.children) expression(child);
    };
    for (const system of declarationsOf(program, 'system')) {
      const statements = (items: HirStmt[]): void => {
        for (const statement of items) {
          for (const value of statement.expressions) expression(value);
          statements(statement.body);
          statements(statement.elseBody);
        }
      };
      statements(system.body);
    }
    return found;
  };
  assert.deepEqual(slots(first.hir), [0, 1]);
  assert.deepEqual(slots(second.hir), [0, 1]);
  assert.equal(first.hir.rngSlotCount, 2);
  assert.equal(first.zir.sim.rngStateCount, 2);
  assert.equal(second.zir.sim.rngStateCount, 2);
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
