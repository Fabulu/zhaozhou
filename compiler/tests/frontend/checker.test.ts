// checker.test.ts — the D6 schedule (one writer per component per phase,
// both spans cited), the E-832 registry-overflow gate, scheduler determinism
// (schedule is a pure function of declaration order + read/write sets), and
// literal exactness arithmetic (Q16.16/Q1.39.24/U 0.0.16/U 0.0.8 laws).

import { test } from 'node:test';
import assert from 'node:assert/strict';

import { compile } from './helpers.js';
import { checkModules } from '../../src/frontend/checker.js';
import { evaluateExactConstant, type ExactConstantBindings } from '../../src/frontend/exact_constant.js';
import { DiagnosticSink } from '../../src/frontend/diagnostics.js';
import type { Expr, ModuleAst, PresentationItem, TopDecl } from '../../src/frontend/ast.js';
import type { SourceSpan } from '../../src/frontend/span.js';

const MOD = (body: string): string => `module m {\n${body}\n}\n`;

test('schedule: one writer per component per phase; readers follow writers', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    global b: fx16 = 0m;
    system w1 every 1 ticks reads writes a { a = 1m; }
    system r1 every 1 ticks reads a writes b { b = a; }
  `));
  assert.deepEqual(r.codes, []);
  const phases = r.check!.schedule!.phases;
  assert.equal(phases.length, 2);
  assert.deepEqual(phases[0]!.systems.map((s) => s.name), ['w1']);
  assert.deepEqual(phases[1]!.systems.map((s) => s.name), ['r1']);
});

test('schedule: declaration order within a phase (deterministic-scheduling §3.5)', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    global b: fx16 = 0m;
    system x every 1 ticks reads writes a { a = 1m; }
    system y every 1 ticks reads writes b { b = 1m; }
    system z every 2 ticks reads writes { }
  `));
  // z has no interactions — shares phase 0 with x and y, in declaration order
  const phases = r.check!.schedule!.phases;
  assert.deepEqual(phases[0]!.systems.map((s) => s.name), ['x', 'y', 'z']);
});

test('schedule: E-500 cites BOTH spans', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    system first every 1 ticks reads writes a { a = 1m; }
    system second every 1 ticks reads writes a { a = 2m; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-500']);
  const msg = r.diagnostics.find((d) => d.code === 'FORM-E-500')!.message;
  assert.ok(msg.includes('first') && msg.includes('second'), 'both systems named');
  assert.ok(/:\d+/.test(msg), 'both spans cited as offsets');
});

test('schedule: multi-rate writer and fast reader interact through phase order only', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    system slow_writer every 4 ticks reads writes a { a = 1m; }
    system fast_reader every 1 ticks reads a writes a { a = a + 1m; }
  `));
  // reader is also a writer of a — conservative ordering puts the fast system
  // after the slow writer; no same-phase double write
  assert.deepEqual(r.codes, []);
  const phases = r.check!.schedule!.phases;
  assert.equal(phases.length, 2);
});

test('schedule: stagger pins one isolated per-entity iteration to every-rate', () => {
  const r = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    system staggered every 4 ticks stagger over p reads p writes p.x {
      for i in 0..p.count { p.x[i] = p.x[i] + 1m; }
    }
  `));
  assert.deepEqual(r.codes, []);
  assert.equal(r.check!.schedule!.phases[0]!.systems[0]!.every, 4n);
});

test('stagger refuses missing iteration, extra loops, and off-loop/global writes', () => {
  const missing = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    system staggered every 4 ticks stagger over p reads p writes p { }
  `));
  assert.deepEqual(missing.codes, ['FORM-E-504']);

  const leaking = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    global g: fx16 = 0m;
    system staggered every 4 ticks stagger over p reads p writes p.x, g {
      g = 1m;
      for i in 0..p.count {
        p.x[i] = p.x[i] + 1m;
        for j in 0..p.count { p.x[j] = p.x[j] + 1m; }
      }
    }
  `));
  assert.ok(leaking.codes.includes('FORM-E-504'));
});

test('stagger refuses persistent RNG before, inside, and after its selected loop', () => {
  const cases = [
    `let draw = random.u32(random.stream(1, 10));
     for i in 0..p.count { p.x[i] = p.x[i] + 1m; }`,
    `for i in 0..p.count {
       if random.u32(random.stream(2, 20)) > 0 { p.x[i] = p.x[i] + 1m; }
       else { p.x[i] = p.x[i] + 2m; }
     }`,
    `for i in 0..p.count { p.x[i] = p.x[i] + 1m; }
     let nested = box { value = random.u32(random.stream(3, 30)) };`,
  ];
  for (const body of cases) {
    const result = compile(MOD(`
      struct s { x: fx16; }
      struct box { value: u32; }
      pool p: s[16];
      system staggered every 4 ticks stagger over p reads p writes p.x {
        ${body}
      }
    `));
    assert.deepEqual(result.codes, ['FORM-E-504'], result.diagnostics.map((d) => d.message).join('\n'));
    assert.match(result.diagnostics[0]!.message, /persistent random stream creation and draws/);
  }
});

test('stagger retains pure expressions around and inside its selected loop', () => {
  const result = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[16];
    system staggered every 4 ticks stagger over p reads p writes p.x {
      let before = min(1m, 2m);
      for i in 0..p.count {
        let inside = max(before, 0m);
        p.x[i] = p.x[i] + inside;
      }
      let after = clamp(before, 0m, 2m);
    }
  `));
  assert.deepEqual(result.codes, [], result.diagnostics.map((d) => d.message).join('\n'));
});

test('E-832: source-ID admission counts rows, not declarations, at exact boundaries', () => {
  const span: SourceSpan = { file: 'generated.form', start: 0, end: 0 };
  const moduleWithRows = (rows: number): ModuleAst => ({
    kind: 'Module', name: 'rows', imports: [], span,
    decls: [{
      kind: 'Presentation', name: 'p', span,
      // Unknown emit kinds are assumed parser-diagnosed; this isolates registry admission.
      items: Array.from({ length: rows }, (): PresentationItem => ({
        kind: 'emit', emitKind: '<registry-probe>', args: [], span,
      })),
    }],
  });
  const codes = (modules: ModuleAst[]): string[] => {
    const sink = new DiagnosticSink();
    checkModules(modules, sink);
    return sink.diagnostics.filter((d) => d.severity === 'error').map((d) => d.code);
  };

  assert.ok(!codes([moduleWithRows(65536)]).includes('FORM-E-832'));
  assert.ok(codes([moduleWithRows(65537)]).includes('FORM-E-832'));

  const declarations: TopDecl[] = Array.from({ length: 70000 }, (_, index) => ({
    kind: 'BadDecl' as const, text: `ignored${index}`, span,
  }));
  assert.ok(!codes([{ kind: 'Module', name: 'declarations', imports: [], decls: declarations, span }]).includes('FORM-E-832'));

  const modules = Array.from({ length: 4097 }, (_, index): ModuleAst => ({
    kind: 'Module', name: `m${index}`, imports: [], decls: [],
    span: { file: `m${index}.form`, start: 0, end: 0 },
  }));
  assert.ok(!codes(modules.slice(0, 4096)).includes('FORM-E-832'));
  assert.ok(codes(modules).includes('FORM-E-832'));
});

test('const admission follows only genuine constant expressions recursively', () => {
  const result = compile(MOD(`
    struct box { value: u32; }
    global runtime_value: u32 = 1;
    fn value() -> u32 { return 2; }
    const FROM_GLOBAL: u32 = runtime_value;
    const FROM_CALL: u32 = value();
    const NESTED_GLOBAL: box = box { value = runtime_value };
    const NESTED_CALL: box = box { value = value() };
    const ZERO_DIVISOR: u32 = 1 / 0;
    const CYCLE_A: u32 = CYCLE_B;
    const CYCLE_B: u32 = CYCLE_A;
    global BAD_START: u32 = runtime_value;
  `));
  assert.equal(result.diagnostics.filter((diagnostic) => diagnostic.code === 'FORM-E-210').length, 8,
    result.diagnostics.map((diagnostic) => `${diagnostic.code}: ${diagnostic.message}`).join('\n'));
});

test('const admission accepts nested records, enum members, and imported constants', () => {
  const result = compile({
    'a_owner.form': `module owner {
  enum mode { ready = 1, done = 2 }
  struct leaf { value: u32; }
  const LEAF: leaf = leaf { value = 3 + 4 };
  const MODE: mode = mode.done;
}\n`,
    'b_consumer.form': `module consumer {
  import owner { leaf, LEAF, mode, MODE };
  struct wrapper { selected: mode; item: leaf; position: world3; }
  const WRAPPED: wrapper = wrapper {
    position = world3 { z = 3, x = 1, y = 2 },
    item = LEAF,
    selected = MODE,
  };
}\n`,
  });
  assert.deepEqual(result.codes, [], result.diagnostics.map((diagnostic) => diagnostic.message).join('\n'));
});

test('struct recursion resolves imported fields in each declaration owner', () => {
  const result = compile({
    'a_owner.form': `module owner {
  struct hidden_leaf { value: u32; }
  struct hidden_record { leaf: hidden_leaf; }
}\n`,
    'b_consumer.form': `module consumer {
  import owner { hidden_record };
  struct hidden_leaf { imported: hidden_record; }
}\n`,
  });
  assert.deepEqual(result.codes, [], result.diagnostics.map((diagnostic) => diagnostic.message).join('\n'));
});

test('struct recursion uses qualified declaration identities and rejects real cycles', () => {
  const sameName = compile({
    'a_owner.form': `module owner {
  struct node { value: u32; }
  struct holder { item: node; }
}\n`,
    'b_consumer.form': `module consumer {
  import owner { holder };
  struct node { item: holder; }
}\n`,
  });
  assert.deepEqual(sameName.codes, [], sameName.diagnostics.map((diagnostic) => diagnostic.message).join('\n'));

  const local = compile(MOD(`
    struct first { next: second; }
    struct second { next: first; }
  `));
  assert.ok(local.codes.includes('FORM-E-801'));
  assert.match(local.diagnostics.find((diagnostic) => diagnostic.code === 'FORM-E-801')!.message,
    /m\.first -> m\.second -> m\.first/);

  const crossModule = compile({
    'a.form': `module a {
  import b { right };
  struct left { next: right; }
}\n`,
    'b.form': `module b {
  import a { left };
  struct right { next: left; }
}\n`,
  });
  assert.ok(crossModule.codes.includes('FORM-E-801'));
  assert.match(crossModule.diagnostics.find((diagnostic) => diagnostic.code === 'FORM-E-801')!.message,
    /a\.left -> b\.right -> a\.left|b\.right -> a\.left -> b\.right/);
});

test('exactness: fx16 steps are 2^-16 — 0.5m exact, 0.5000001m is FORM-E-008', () => {
  assert.deepEqual(compile(MOD('  const A: fx16 = 0.5m;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx16 = 0.5000001m;')).codes, ['FORM-E-008']);
});

test('exactness: fx24 steps are 2^-24; angle16 wraps mod 1 turn; unit8 rounds half-up', () => {
  assert.deepEqual(compile(MOD('  const A: fx24 = 2.25w;')).codes, []);
  // 2^-24 = 0.000000059604644775390625 — representable; 1e-7 is not
  assert.deepEqual(compile(MOD('  const A2: fx24 = 0.000000059604644775390625w;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx24 = 0.0000001w;')).codes, ['FORM-E-008']);
  // 2.5 turns wraps to 0.5 turns — no range error for angle16 (U 0.0.16 wraps)
  assert.deepEqual(compile(MOD('  const C: angle16 = 2.5turn;')).codes, []);
  // 1deg is not an exact 1/65536 multiple -> FORM-E-008
  assert.deepEqual(compile(MOD('  const D: angle16 = 1deg;')).codes, ['FORM-E-008']);
  // 90deg = 0.25 turn exact
  assert.deepEqual(compile(MOD('  const E: angle16 = 90deg;')).codes, []);
  // unit8 percent rounds by law (raw = (pct*256+50)/100) — never FORM-E-008
  assert.deepEqual(compile(MOD('  const F: unit8 = 33%;')).codes, []);
});

test('exactness: fx16 range is [-32768, 32768) — FORM-E-007 outside', () => {
  assert.deepEqual(compile(MOD('  const A: fx16 = 32767.5m;')).codes, []);
  assert.deepEqual(compile(MOD('  const B: fx16 = 32768m;')).codes, ['FORM-E-007']);
});

test('Q-format rails and huge fractions use exact rational comparisons', () => {
  const huge = '0'.repeat(310);
  const hugeDenominator = '0'.repeat(400);
  const accepted = compile(MOD(`
    const F16_LO: fx16 = -32768m;
    const F16_HI: fx16 = 32767.9999847412109375m;
    const F16_STEP: fx16 = 0.0000152587890625m;
    const F24_FAR: fx24 = 10000w;
    const F24_INT_LO: fx24 = -549755813888;
    const F24_INT_HI: fx24 = 549755813887;
    const F24_LO: fx24 = -549755813888w;
    const F24_HI: fx24 = 549755813887.999999940395355224609375w;
    const F24_STEP: fx24 = 0.000000059604644775390625w;
    const HUGE_ONE: fx16 = 1.${hugeDenominator}m;
    const HUGE_ZERO: fx24 = 0.${hugeDenominator}w;
  `));
  assert.deepEqual(accepted.codes, [], accepted.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));

  for (const declaration of [
    `const BAD: fx16 = 32768.${huge}m;`,
    'const BAD: fx16 = -32768.0000152587890625m;',
    `const BAD: fx24 = 549755813888.${huge}w;`,
    'const BAD: fx24 = -549755813888.000000059604644775390625w;',
    'const BAD: fx24 = 549755813888;',
    'const BAD: fx24 = -549755813889;',
    `const BAD: fx24 = ${'9'.repeat(400)}w;`,
    `const BAD: fx24 = -${'9'.repeat(400)}w;`,
  ]) {
    assert.ok(compile(MOD(declaration)).codes.includes('FORM-E-007'), declaration);
  }
  const tiny = compile(MOD(`const TINY: fx16 = 0.${'0'.repeat(399)}1m;`));
  assert.ok(tiny.codes.includes('FORM-E-008'));
  assert.equal(tiny.codes.includes('FORM-E-007'), false);
});

test('space-typing: velocity3 never mixes into world3 arithmetic (FORM-E-330)', () => {
  const r = compile(MOD(`
    fn f(p: world3, v: velocity3) -> world3 { return p + v; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-330']);
});

test('present purity: presentation blocks write nothing, ever (domains §2.2)', () => {
  const r = compile(MOD(`
    global a: fx16 = 0m;
    presentation p { a = 1m; }
  `));
  assert.deepEqual(r.codes, ['FORM-E-405']);
});

test('field dialect: fx24 never appears inside a field program (Q2)', () => {
  const r = compile(MOD(`
    @earth field f() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 32 {
      let wide = 2.25w;
      return terrain_delta { height = 0m, velocity = 0m, material = 0, nav_cost = 0m };
    }
  `));
  assert.ok(r.codes.includes('FORM-E-332'), r.codes.join(','));
});

test('one-writer analysis feeds W3.3: the schedule shape is committed data', () => {
  // mirrors the positive corpus ordering: input_latch -> spawn_waves -> integrate
  const r = compile(MOD(`
    struct s { x: fx16; }
    pool p: s[4];
    global pos: fx16 = 0m;
    global out: fx16 = 0m;
    system latch every 1 ticks reads input writes pos { pos = 0m; }
    system spawner every 60 ticks reads pos writes p { spawn(p, s { x = pos }); }
    system mover every 1 ticks reads p writes out { for c in p { out = c.x; } }
  `));
  assert.deepEqual(r.codes, [], r.diagnostics.map((d) => d.code + ' ' + d.message).join('\n'));
  const names = r.check!.schedule!.phases.map((ph) => ph.systems.map((s) => s.name));
  assert.deepEqual(names, [['latch'], ['spawner'], ['mover']]);
});

test('compositional member/index typing preserves ordinary aggregates and pool columns', () => {
  const result = compile(MOD(`
    struct inner { values: u32[2]; }
    struct bag { nested: inner; values: u32[2]; vector: world3; }
    pool things: bag[4];
    global origin: world3 = world3 { x = 1, y = 2, z = 3 };
    global answer: u32 = 0;
    fn pick(item: bag) -> u32 {
      return item.values[0] + item.nested.values[1];
    }
    system compose every 1 ticks reads origin, things.vector, things.values writes answer {
      let x = origin.x;
      let px = things.vector[0].x;
      let value = things.values[0][1];
      answer = value;
    }
  `));
  assert.deepEqual(result.codes, [], result.diagnostics.map((d) => d.message).join('\n'));
});

test('owner-qualified scheduler keys separate unrelated state and converge imports', () => {
  const independent = compile({
    'a.form': `module a {
      global state: u32 = 0;
      system write_a every 1 ticks reads writes state { state = 1; }
    }\n`,
    'b.form': `module b {
      global state: u32 = 0;
      system write_b every 1 ticks reads writes state { state = 2; }
    }\n`,
  });
  assert.deepEqual(independent.codes, [], independent.diagnostics.map((d) => d.message).join('\n'));
  assert.deepEqual(independent.check!.schedule!.phases[0]!.systems.map((s) => s.name), ['write_a', 'write_b']);

  const imported = compile({
    'a.form': `module a {
      global state: u32 = 0;
      system write_a every 1 ticks reads writes state { state = 1; }
    }\n`,
    'b.form': `module b {
      import a;
      global seen: u32 = 0;
      system read_a every 1 ticks reads a.state writes seen { seen = a.state; }
    }\n`,
  });
  assert.deepEqual(imported.codes, [], imported.diagnostics.map((d) => d.message).join('\n'));
  assert.deepEqual(imported.check!.schedule!.phases.map((p) => p.systems.map((s) => s.name)), [['write_a'], ['read_a']]);

  const conflict = compile({
    'a.form': `module a { global state: u32 = 0; }\n`,
    'b.form': `module b {
      import a;
      system first every 1 ticks reads writes a.state { a.state = 1; }
      system second every 1 ticks reads writes a.state { a.state = 2; }
    }\n`,
  });
  assert.ok(conflict.codes.includes('FORM-E-500'));
});

test('whole-module qualification is shared by types, values, enums, calls, pools and accesses', () => {
  const result = compile({
    'owner.form': `module owner {
      enum mode { idle = 0, ready = 1 }
      struct item { count: u32; capacity: u32; value: u32; }
      const LIMIT: u32 = 4;
      global marker: u32 = 1;
      pool items: item[LIMIT];
      fn inc(value: u32) -> u32 { return value + 1; }
    }\n`,
    'consumer.form': `module consumer {
      import owner;
      global output: u32 = 0;
      fn classify(value: owner.item, state: owner.mode) -> u32 {
        if state == owner.mode.ready { return owner.inc(value.value); }
        else { return owner.LIMIT; }
      }
      system use every 1 ticks reads owner.marker, owner.items.count writes output, owner.items.value {
        let created = owner.item { value = owner.marker, count = 0, capacity = owner.LIMIT };
        let selected = classify(created, owner.mode.ready);
        let authored_count = owner.items.count[0];
        owner.items.value[0] = authored_count;
        output = selected;
      }
    }\n`,
  });
  assert.deepEqual(result.codes, [], result.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
});

test('qualified global reads still violate pure-function effects', () => {
  const result = compile({
    'owner.form': `module owner { global leaked: u32 = 1; }\n`,
    'consumer.form': `module consumer {
      import owner;
      fn leak() -> u32 { return owner.leaked; }
    }\n`,
  });
  assert.ok(result.codes.includes('FORM-E-402'));
});

test('definite return requires every path and never trusts an ordinary loop', () => {
  assert.deepEqual(compile(MOD(`
    fn complete(flag: bool, other: bool) -> u32 {
      if flag { if other { return 1; } else { return 2; } }
      else { return 3; }
    }
  `)).codes, []);
  assert.ok(compile(MOD(`
    fn falls_through(flag: bool) -> u32 { if flag { return 1; } }
  `)).codes.includes('FORM-E-310'));
  assert.ok(compile(MOD(`
    fn loop_only() -> u32 { for i in 0..1 { return i; } }
  `)).codes.includes('FORM-E-310'));
});

test('declaration numeric bounds reject overflow before Number conversion', () => {
  assert.deepEqual(compile(MOD(`
    struct item { x: u32; }
    const MAX: u32 = 4294967295;
    struct arrays { values: u32[MAX]; }
    pool maximal: item[MAX];
    enum edge { zero = 0, top = 4294967295 }
  `)).codes.filter((code) => ['FORM-E-007', 'FORM-E-800', 'FORM-E-801'].includes(code)), []);
  assert.ok(compile(MOD(`struct item { x: u32; } pool too_large: item[4294967296];`)).codes.includes('FORM-E-800'));
  assert.ok(compile(MOD(`enum overflow { value = 4294967296 }`)).codes.includes('FORM-E-007'));
  assert.ok(compile(MOD(`global values: u32[4294967296] = 0;`)).codes.includes('FORM-E-801'));
  assert.ok(compile(MOD(`system huge every 4294967296 ticks reads writes { }`)).codes.includes('FORM-E-506'));
});

test('exact bound reduction handles unary width operations and diagnoses irreducible bounds', () => {
  const accepted = compile(MOD(`
    const ZERO: u32 = 0;
    const MASK: u32 = ~ZERO;
    const CAP: u32 = MASK >> 30;
    const WRAPPED: u32 = 4294967295 + 2;
    const SIGNED: i32 = ~0;
    struct row { values: u32[CAP]; }
    pool rows: row[CAP];
  `));
  assert.deepEqual(accepted.codes, [], accepted.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));

  const refused = compile(MOD(`
    const ZERO: u32 = 0;
    const BAD: u32 = 1 / ZERO;
    struct row { values: u32[BAD]; }
    pool rows: row[BAD];
  `));
  assert.ok(refused.codes.includes('FORM-E-800'));
  assert.ok(refused.codes.includes('FORM-E-801'));
});

test('exact aggregate projections feed bounds across nesting, forward references, and owners', () => {
  const local = compile(MOD(`
    struct leaf { cap: u32; }
    struct metadata { nested: leaf; }
    const FORWARD: u32 = LATER.nested.cap;
    const LATER: metadata = metadata { nested = leaf { cap = 3 } };
    const DIRECT: u32 = (metadata { nested = leaf { cap = 4 } }).nested.cap;
    const CAP: u32 = FORWARD + DIRECT;
    struct row { values: u32[CAP]; }
    pool rows: row[CAP];
  `));
  assert.deepEqual(local.codes, [], local.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));

  const qualified = compile({
    'owner.form': `module owner {
      struct leaf { cap: u32; }
      struct metadata { nested: leaf; }
      const META: metadata = metadata { nested = leaf { cap = 5 } };
    }\n`,
    'consumer.form': `module consumer {
      import owner;
      const CAP: u32 = owner.META.nested.cap;
      struct row { values: u32[CAP]; }
      pool rows: row[CAP];
    }\n`,
  });
  assert.deepEqual(qualified.codes, [], qualified.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
});

test('shared exact reducer projects nested fixed-array records and refuses invalid indices', () => {
  const span: SourceSpan = { file: 'exact.form', start: 0, end: 1 };
  const values: Expr = { kind: 'ident', name: 'VALUES', span };
  const projection = (index: bigint): Expr => ({
    kind: 'member', field: 'cap', fieldSpan: span, span,
    obj: {
      kind: 'index', obj: values, span,
      index: { kind: 'literal', lit: 'int', text: index.toString(), intVal: index, span },
    },
  });
  const arrayType = { t: 'array' };
  const rowType = { t: 'record' };
  const u32Type = { t: 'u32' };
  const bindings: ExactConstantBindings<number> = {
    typeOf: (_context, expression) => expression === values ? arrayType
      : expression.kind === 'index' ? rowType
        : expression.kind === 'member' || expression.kind === 'literal' ? u32Type : null,
    constant: () => null,
    enumMember: () => null,
    memberType: (_context, aggregate, field) => aggregate.t === 'record' && field === 'cap' ? u32Type : null,
    elementType: (_context, aggregate) => aggregate.t === 'array' ? rowType : null,
    aggregateValue: (_context, expression) => expression === values ? {
      kind: 'array',
      elements: [
        { kind: 'record', fields: new Map([['cap', 3n]]) },
        { kind: 'record', fields: new Map([['cap', 7n]]) },
      ],
    } : null,
  };
  assert.equal(evaluateExactConstant(0, projection(1n), u32Type, bindings), 7n);
  assert.equal(evaluateExactConstant(0, projection(2n), u32Type, bindings), null);
  assert.equal(evaluateExactConstant(0, projection(-1n), u32Type, bindings), null);
});

test('aggregate bounds diagnose cycles, missing members, and field-type drift', () => {
  const cyclic = compile(MOD(`
    struct metadata { cap: u32; }
    struct row { value: u32; }
    const META: metadata = metadata { cap = CAP };
    const CAP: u32 = META.cap;
    pool rows: row[CAP];
  `));
  assert.ok(cyclic.codes.includes('FORM-E-210'));
  assert.ok(cyclic.codes.includes('FORM-E-800'));

  const missing = compile(MOD(`
    struct metadata { cap: u32; }
    struct row { value: u32; }
    const META: metadata = metadata { cap = 3 };
    const CAP: u32 = META.missing;
    pool rows: row[CAP];
  `));
  assert.ok(missing.codes.includes('FORM-E-306'));
  assert.ok(missing.codes.includes('FORM-E-800'));

  const wrongType = compile(MOD(`
    struct metadata { cap: fx16; }
    struct row { value: u32; }
    const META: metadata = metadata { cap = 3m };
    const CAP: u32 = META.cap;
    pool rows: row[CAP];
  `));
  assert.ok(wrongType.codes.includes('FORM-E-300'));
  assert.equal(wrongType.codes.includes('FORM-E-800'), false,
    'the declaration-type error, not an invented bound value, owns this refusal');
});

test('whole-module-qualified flow calls share selective targets and qualified pool effects', () => {
  const source = (whole: boolean): Record<string, string> => ({
    'a_data.form': `module data {
      struct particle { position: world3; velocity: velocity3; age: u32; }
      pool motes: particle[4];
    }\n`,
    'b_flowlib.form': `module flowlib {
      @flow field drift() -> flow_update footprint none; max_ops 48 {
        return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0m };
      }
    }\n`,
    'c_consumer.form': `module consumer {
      import data;
      import flowlib${whole ? '' : ' { drift }'};
      system move every 1 ticks reads data.motes writes data.motes {
        ${whole ? 'flowlib.drift' : 'drift'}(data.motes);
      }
    }\n`,
  });
  const qualified = compile(source(true));
  const selective = compile(source(false));
  assert.deepEqual(qualified.codes, [], qualified.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  assert.deepEqual(selective.codes, [], selective.diagnostics.map((d) => `${d.code}: ${d.message}`).join('\n'));
  const target = {
    kind: 'field', module: 'flowlib', name: 'drift',
    flowPool: { module: 'data', name: 'motes' },
  };
  assert.deepEqual([...qualified.check!.callTargets.values()], [target]);
  assert.deepEqual([...selective.check!.callTargets.values()], [target]);
  assert.deepEqual([...qualified.check!.accessKeys.values()], ['data\0motes', 'data\0motes']);
  assert.deepEqual([...selective.check!.accessKeys.values()], ['data\0motes', 'data\0motes']);
  const scheduled = (result: typeof qualified): unknown => {
    const system = result.check!.schedule!.phases
      .flatMap((phase) => phase.systems)
      .find((item) => item.name === 'move')!;
    return { name: system.name, module: system.module, every: system.every };
  };
  assert.deepEqual(scheduled(qualified), scheduled(selective));
});

test('lexical qualifier shadows and selective ambiguity never become canonical calls', () => {
  const shadowed = compile({
    'a_data.form': `module data {
      struct particle { position: world3; velocity: velocity3; age: u32; }
      pool motes: particle[4];
    }\n`,
    'b_flowlib.form': `module flowlib {
      @flow field drift() -> flow_update footprint none; max_ops 48 {
        return flow_update { x = p.x, y = p.y, z = p.z, vx = p.vx, vy = p.vy, vz = p.vz, attr0 = 0m };
      }
    }\n`,
    'c_consumer.form': `module consumer {
      import data;
      import flowlib;
      system move every 1 ticks reads data.motes writes data.motes {
        let flowlib: u32 = 0;
        flowlib.drift(data.motes);
      }
    }\n`,
  });
  assert.ok(shadowed.codes.includes('FORM-E-306'));
  assert.ok(shadowed.codes.includes('FORM-E-110'));
  assert.deepEqual([...shadowed.check!.callTargets.values()], []);

  const ambiguous = compile({
    'a.form': `module a { fn drift(value: u32) -> u32 { return value; } }\n`,
    'b.form': `module b { fn drift(value: u32) -> u32 { return value; } }\n`,
    'c.form': `module c {
      import a { drift };
      import b { drift };
      fn choose() -> u32 { return drift(1); }
    }\n`,
  });
  assert.ok(ambiguous.codes.includes('FORM-E-205'));
  assert.deepEqual([...ambiguous.check!.callTargets.values()], []);

  const missing = compile({
    'flowlib.form': `module flowlib { fn present() -> u32 { return 1; } }\n`,
    'user.form': `module user { import flowlib; fn broken() -> u32 { return flowlib.missing(); } }\n`,
  });
  assert.ok(missing.codes.includes('FORM-E-203'));
  assert.deepEqual([...missing.check!.callTargets.values()], []);
});

test('bare calls honor lexical shadows while explicit module qualification escapes them', () => {
  const result = compile({
    'a_library.form': `module library {
      fn invoke(value: u32) -> u32 { return value; }
    }\n`,
    'b_consumer.form': `module consumer {
      import library;
      import library { invoke };
      fn parameter(invoke: u32) -> u32 { return invoke(1); }
      fn current() -> u32 { let invoke: u32 = 0; return invoke(1); }
      fn future() -> u32 {
        let result: u32 = invoke(1);
        let invoke: u32 = 0;
        return result;
      }
      fn escape(invoke: u32) -> u32 { return library.invoke(invoke); }
    }\n`,
  });
  assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-110').length, 2);
  assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1);
  assert.equal(result.codes.includes('FORM-E-203'), false);
  assert.deepEqual([...result.check!.callTargets.values()], [{
    kind: 'fn', module: 'library', name: 'invoke', flowPool: null,
  }]);
});

test('non-callable declarations shadow selectively imported bare callables', () => {
  const declarations = [
    'const invoke: u32 = 0;',
    'global invoke: u32 = 0;',
    'struct row { value: u32; } pool invoke: row[1];',
  ];
  for (const declaration of declarations) {
    const result = compile({
      'a_library.form': `module library {
        fn invoke(value: u32) -> u32 { return value; }
      }\n`,
      'b_consumer.form': `module consumer {
        import library { invoke };
        ${declaration}
        fn probe() -> u32 { return invoke(1); }
      }\n`,
    });
    assert.ok(result.codes.includes('FORM-E-110'), declaration);
    assert.equal(result.codes.includes('FORM-E-203'), false, declaration);
    assert.deepEqual([...result.check!.callTargets.values()], [], declaration);
  }
});

test('every bare intrinsic spelling yields to a current-module function', () => {
  const names = [...new Set([
    'abs', 'min', 'max', 'clamp', 'sin', 'cos', 'atan2_approx', 'sqrt_approx',
    'to_fx16', 'to_fx24', 'to_unit8', 'to_angle16', 'dot2', 'dot3', 'length',
    'normalize', 'mix', 'dist', 'length2', 'length3', 'normalize2', 'normalize3',
    'rcp', 'curve', 'spline', 'dcurve', 'noise2', 'ridge', 'ring', 'rot2',
    'rot3', 'smoothstep',
  ])];
  const result = compile(MOD(`
    ${names.map((name) => `fn ${name}() -> u32 { return 5; }`).join('\n')}
    fn probe() -> u32 { return ${names.map((name) => `${name}()`).join(' + ')}; }
  `));
  assert.deepEqual(result.codes, [], result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  const targets = [...result.check!.callTargets.values()];
  assert.equal(targets.length, names.length);
  assert.deepEqual(targets.map((target) => target.kind), names.map(() => 'fn'));
  assert.deepEqual(targets.map((target) => target.kind === 'intrinsic' ? target.name : target.name), names);
});

test('ordinary imports, ambiguities, and non-callables precede intrinsic fallback', () => {
  const selective = compile({
    'a.form': `module a { fn min(a: u32, b: u32) -> u32 { return a + b; } }\n`,
    'b.form': `module b { import a { min }; fn probe() -> u32 { return min(2, 3); } }\n`,
  });
  assert.deepEqual(selective.codes, [], selective.diagnostics.map((item) => item.message).join('\n'));
  assert.deepEqual([...selective.check!.callTargets.values()], [{
    kind: 'fn', module: 'a', name: 'min', flowPool: null,
  }]);

  const nonCallable = compile(MOD(`
    const min: u32 = 0;
    fn probe() -> u32 { return min(2, 3); }
  `));
  assert.ok(nonCallable.codes.includes('FORM-E-110'));
  assert.deepEqual([...nonCallable.check!.callTargets.values()], []);

  const ambiguous = compile({
    'a.form': `module a { fn min(a: u32, b: u32) -> u32 { return a; } }\n`,
    'b.form': `module b { fn min(a: u32, b: u32) -> u32 { return b; } }\n`,
    'c.form': `module c { import a { min }; import b { min }; fn probe() -> u32 { return min(2, 3); } }\n`,
  });
  assert.ok(ambiguous.codes.includes('FORM-E-205'));
  assert.deepEqual([...ambiguous.check!.callTargets.values()], []);
});

test('module-qualified intrinsic-like member names remain ordinary callable identities', () => {
  const result = compile({
    'a_library.form': `module library {
      fn min() -> u32 { return 2; }
      fn abs() -> u32 { return 3; }
    }\n`,
    'b_app.form': `module app {
      import library;
      fn probe() -> u32 { return library.min() + library.abs(); }
    }\n`,
  });
  assert.deepEqual(result.codes, [], result.diagnostics.map((item) => item.message).join('\n'));
  assert.deepEqual([...result.check!.callTargets.values()], [
    { kind: 'fn', module: 'library', name: 'min', flowPool: null },
    { kind: 'fn', module: 'library', name: 'abs', flowPool: null },
  ]);
});

test('future lets reserve enum, pool, aggregate, nested, and call-qualifier roots', () => {
  const result = compile({
    'a_owner.form': `module owner { fn value() -> u32 { return 7; } }\n`,
    'b_app.form': `module app {
      import owner;
      enum mode { ready = 1 }
      struct inner { value: u32; }
      struct row { field: u32; nested: inner; }
      pool items: row[2];
      global box: row = row { field = 1, nested = inner { value = 2 } };
      fn enum_root() -> bool {
        let before: bool = mode.ready == mode.ready;
        let mode: u32 = 0;
        return before;
      }
      fn pool_metadata() -> u32 {
        let before: u32 = items.count;
        let items: u32 = 0;
        return before;
      }
      fn pool_field() -> u32 {
        let before: u32 = items.field[0];
        let items: u32 = 0;
        return before;
      }
      fn aggregate_root() -> u32 {
        let before: u32 = box.field;
        let box: u32 = 0;
        return before;
      }
      fn nested_root() -> u32 {
        let before: u32 = box.nested.value;
        let box: u32 = 0;
        return before;
      }
      fn call_qualifier() -> u32 {
        let before: u32 = owner.value();
        let owner: u32 = 0;
        return before;
      }
    }\n`,
  });
  assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 7,
    result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  assert.deepEqual([...result.check!.callTargets.values()], []);
});

test('every executable lexical body reserves its own direct future lets', () => {
  const cases: Record<string, string>[] = [
    {
      'earth.form': `module app {
        @earth field probe() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 32 {
          let before = min(1m, 2m);
          let min = 3m;
          return terrain_delta { height = before, velocity = 0m, material = 0, nav_cost = 0m };
        }
      }\n`,
    },
    {
      'flow.form': `module app {
        @flow field probe() -> flow_update footprint none; max_ops 48 {
          let before = max(1m, 2m);
          let max = 3m;
          return flow_update { x = before, y = 0m, z = 0m, vx = 0m, vy = 0m, vz = 0m, attr0 = 0m };
        }
      }\n`,
    },
    {
      'function.form': `module app {
        fn probe() -> u32 { let before: u32 = min(1, 2); let min: u32 = 3; return before; }
      }\n`,
    },
    {
      'system.form': `module app {
        system probe every 1 ticks reads writes {
          let before: u32 = min(1, 2);
          let min: u32 = 3;
        }
      }\n`,
    },
    {
      'nested.form': `module app {
        fn probe(flag: bool) -> u32 {
          if flag { let before: u32 = min(1, 2); let min: u32 = 3; }
          return 0;
        }
      }\n`,
    },
    {
      'nested_outer.form': `module app {
        fn probe(flag: bool) -> u32 {
          if flag { let before: u32 = min(1, 2); }
          let min: u32 = 3;
          return min;
        }
      }\n`,
    },
  ];

  for (const sources of cases) {
    const result = compile(sources);
    assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1,
      result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
    assert.deepEqual([...result.check!.callTargets.values()], []);
  }
});

test('nested lexical bodies neither reserve nor leak names into unrelated scopes', () => {
  const result = compile(MOD(`
    @earth field earth_ok() -> terrain_delta footprint rect(0m, 0m, 1m, 1m); max_ops 32 {
      let value = min(1m, 2m);
      return terrain_delta { height = value, velocity = 0m, material = 0, nav_cost = 0m };
    }
    @flow field flow_ok() -> flow_update footprint none; max_ops 48 {
      let value = max(1m, 2m);
      return flow_update { x = value, y = 0m, z = 0m, vx = 0m, vy = 0m, vz = 0m, attr0 = 0m };
    }
    fn nested(flag: bool) -> u32 {
      let before: u32 = min(4, 5);
      if flag { let min: u32 = 3; }
      else { let branch: u32 = min(5, 6); }
      let after: u32 = min(6, 7);
      for i in 0..1 { let max: u32 = 2; }
      let tail: u32 = max(before, after);
      return tail;
    }
    system stepper every 1 ticks reads writes { let value: u32 = min(2, 3); }
    scenario script { seed 1; assert min(2, 3) == 2; }
  `));
  assert.deepEqual(result.codes, [], result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  assert.deepEqual([...result.check!.callTargets.values()].map((target) =>
    target.kind === 'intrinsic' ? target.name : `${target.module}.${target.name}`),
  ['min', 'max', 'min', 'min', 'min', 'max', 'min', 'min']);
});

test('specialized pool operands obey lexical-root precedence and record canonical targets', () => {
  const data = `module data { struct row { value: u32; } pool items: row[2]; }\n`;
  const rejectedBodies = [
    'spawn(data.items, data.row { value = 1 }); let data: u32 = 0;',
    'kill(data.items, 0); let data: u32 = 0;',
    'for item in data.items { } let data: u32 = 0;',
    'if true { kill(data.items, 0); } let data: u32 = 0;',
  ];
  for (const body of rejectedBodies) {
    const result = compile({
      'data.form': data,
      'app.form': `module app {
        import data;
        system step every 1 ticks reads writes data.items { ${body} }
      }\n`,
    });
    assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1,
      result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
    assert.equal(result.check!.declarationTargets.size, 0);
  }

  const recordRoot = compile({
    'data.form': data,
    'app.form': `module app {
      import data { items, row };
      system step every 1 ticks reads writes items {
        spawn(items, data.row { value = 1 });
        let data: u32 = 0;
      }
    }\n`,
  });
  assert.equal(recordRoot.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1);
  assert.equal(recordRoot.check!.declarationTargets.size, 0);

  const selectiveFuture = compile({
    'data.form': data,
    'app.form': `module app {
      import data { items, row };
      system step every 1 ticks reads writes items {
        spawn(items, row { value = 1 });
        let items: u32 = 0;
      }
    }\n`,
  });
  assert.equal(selectiveFuture.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1);
  assert.equal(selectiveFuture.check!.declarationTargets.size, 0);

  const localRoot = compile({
    'data.form': data,
    'app.form': `module app {
      import data;
      system step every 1 ticks reads writes data.items {
        let data: u32 = 0;
        kill(data.items, 0);
      }
    }\n`,
  });
  assert.equal(localRoot.diagnostics.some((item) => item.code === 'FORM-E-303'), false);
  assert.equal(localRoot.diagnostics.filter((item) => item.code === 'FORM-E-203').length, 1);
  assert.match(localRoot.diagnostics.find((item) => item.code === 'FORM-E-203')!.message,
    /resolves through local 'data', not a declared pool/);

  const legalQualified = compile({
    'data.form': data,
    'app.form': `module app {
      import data;
      system step every 1 ticks reads data.items writes data.items {
        spawn(data.items, data.row { value = 1 });
        for item in data.items { let copy: u32 = item.value; }
        kill(data.items, 0);
      }
    }\n`,
  });
  assert.deepEqual(legalQualified.codes, [],
    legalQualified.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  assert.deepEqual([...legalQualified.check!.declarationTargets.values()], [
    { kind: 'pool', module: 'data', name: 'items', element: { module: 'data', name: 'row' } },
    { kind: 'struct', module: 'data', name: 'row' },
    { kind: 'pool', module: 'data', name: 'items', element: { module: 'data', name: 'row' } },
    { kind: 'pool', module: 'data', name: 'items', element: { module: 'data', name: 'row' } },
  ]);

  const legalSelective = compile({
    'data.form': data,
    'app.form': `module app {
      import data { items, row };
      system step every 1 ticks reads items writes items {
        spawn(items, row { value = 1 });
        for item in items { let copy: u32 = item.value; }
        kill(items, 0);
      }
    }\n`,
  });
  assert.deepEqual(legalSelective.codes, [],
    legalSelective.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  assert.deepEqual([...legalSelective.check!.declarationTargets.values()],
    [...legalQualified.check!.declarationTargets.values()]);
});

test('field-application declaration operands obey future-let reservation', () => {
  const field = `module fields {
    @earth field lift() -> terrain_delta footprint circle(0m, 0m, 1m); max_ops 16 {
      return terrain_delta { height = 1m, velocity = 0m, material = 0, nav_cost = 0m };
    }
  }\n`;
  const rejected = compile({
    'fields.form': field,
    'app.form': `module app {
      import fields;
      system step every 1 ticks reads writes terrain {
        apply terrain_field fields.lift(origin: world2 { x = 0w, y = 0w }) duration 1t;
        let fields: u32 = 0;
      }
    }\n`,
  });
  assert.equal(rejected.diagnostics.filter((item) => item.code === 'FORM-E-303').length, 1);
  assert.equal(rejected.check!.declarationTargets.size, 0);

  const accepted = compile({
    'fields.form': field,
    'app.form': `module app {
      import fields;
      system step every 1 ticks reads writes terrain {
        apply terrain_field fields.lift(origin: world2 { x = 0w, y = 0w }) duration 1t;
      }
    }\n`,
  });
  assert.deepEqual(accepted.codes, [],
    accepted.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  assert.deepEqual([...accepted.check!.declarationTargets.values()], [
    { kind: 'field', module: 'fields', name: 'lift' },
  ]);
});

test('checked loop-bound reduction never reinterprets lexical names as constants', () => {
  const cases: Record<string, string>[] = [
    {
      'local.form': `module app {
        const LIMIT: u32 = 1;
        global runtime_limit: u32 = 4294967295;
        global steps: u32 = 0;
        system run every 1 ticks reads runtime_limit writes steps {
          let LIMIT: u32 = runtime_limit;
          for i in 0..LIMIT { steps = i; }
        }
      }\n`,
    },
    {
      'parameter.form': `module app {
        const LIMIT: u32 = 1;
        fn probe(LIMIT: u32) -> u32 { for i in 0..LIMIT { } return 0; }
      }\n`,
    },
    {
      'loop_index.form': `module app {
        const LIMIT: u32 = 1;
        fn probe() -> u32 { for LIMIT in 0..1 { for i in 0..LIMIT { } } return 0; }
      }\n`,
    },
    {
      'future.form': `module app {
        const LIMIT: u32 = 1;
        global runtime: u32 = 2;
        system probe every 1 ticks reads runtime writes {
          for i in 0..LIMIT { }
          let LIMIT: u32 = runtime;
        }
      }\n`,
    },
    {
      'global.form': `module app {
        global runtime_limit: u32 = 4294967295;
        system probe every 1 ticks reads runtime_limit writes { for i in 0..runtime_limit { } }
      }\n`,
    },
    {
      'defs.form': 'module defs { const LIMIT: u32 = 1; }\n',
      'selective.form': `module app {
        import defs { LIMIT };
        fn probe(LIMIT: u32) -> u32 { for i in 0..LIMIT { } return 0; }
      }\n`,
    },
    {
      'defs.form': 'module defs { const LIMIT: u32 = 1; }\n',
      'whole.form': `module app {
        import defs;
        fn probe(runtime: u32) -> u32 {
          let defs: u32 = runtime;
          for i in 0..defs.LIMIT { }
          return 0;
        }
      }\n`,
    },
  ];
  for (const sources of cases) {
    const result = compile(sources);
    assert.equal(result.diagnostics.filter((item) => item.code === 'FORM-E-502').length, 1,
      result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
  }
});

test('context-sensitive constant probes honor checked lexical bindings outside loops', () => {
  const duration = compile(MOD(`
    const DURATION: u32 = 1t;
    global runtime: u32 = 2;
    @earth field lift() -> terrain_delta footprint circle(0m, 0m, 1m); max_ops 16 {
      return terrain_delta { height = 1m, velocity = 0m, material = 0, nav_cost = 0m };
    }
    system apply_step every 1 ticks reads runtime writes terrain {
      let DURATION: u32 = runtime;
      apply terrain_field lift(origin: world2 { x = 0w, y = 0w }) duration DURATION;
    }
  `));
  assert.equal(duration.diagnostics.filter((item) => item.code === 'FORM-E-308').length, 1,
    duration.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));

  const player = compile(MOD(`
    const PLAYER: u32 = 9;
    system sample_input every 1 ticks reads input writes {
      let PLAYER: u32 = 0;
      let pad = input.player(PLAYER);
    }
  `));
  assert.deepEqual(player.codes, [],
    player.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));

  const stagger = compile(MOD(`
    const START: u32 = 0;
    global runtime_start: u32 = 0;
    struct row { value: fx16; }
    pool items: row[4];
    system staggered every 2 ticks stagger over items reads runtime_start, items writes items.value {
      let START: u32 = runtime_start;
      for i in START..items.count { items.value[i] = items.value[i] + 1m; }
    }
  `));
  assert.deepEqual(stagger.codes, ['FORM-E-504'],
    stagger.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
});

test('unshadowed exact constants and aggregate projections remain loop bounds', () => {
  const result = compile({
    'defs.form': `module defs {
      struct bounds { high: u32; }
      const LIMIT: u32 = 2;
      const BOUNDS: bounds = bounds { high = 3 };
    }\n`,
    'app.form': `module app {
      import defs;
      const LOCAL: u32 = 1;
      fn probe() -> u32 {
        for a in 0..LOCAL { }
        for b in 0..defs.LIMIT { }
        for c in 0..defs.BOUNDS.high { }
        return 0;
      }
    }\n`,
  });
  assert.deepEqual(result.codes, [],
    result.diagnostics.map((item) => `${item.code}: ${item.message}`).join('\n'));
});

test('comparison admission matrix accepts scalars and enums, rejects aggregates and handles', () => {
  const accepted = compile(MOD(`
    enum mode { low = 0, high = 1 }
    fn all(a: fx16, b: fx24, c: angle16, d: unit8, i: i32, u: u32,
           flag: bool, colour: colour8, left: mode, right: mode) -> bool {
      return a < a && b <= b && c > c && d >= d && i < i && u < u
        && flag == flag && colour != colour && left < right;
    }
  `));
  assert.deepEqual(accepted.codes, [], accepted.diagnostics.map((d) => d.message).join('\n'));
  assert.ok(compile(MOD(`fn bad(a: world3) -> bool { return a == a; }`)).codes.includes('FORM-E-300'));
  assert.ok(compile(MOD(`struct pair { x: u32; } fn bad(a: pair) -> bool { return a < a; }`)).codes.includes('FORM-E-300'));
  assert.ok(compile(MOD(`fn bad(s: bool) -> bool { return s < s; }`)).codes.includes('FORM-E-300'));
});
